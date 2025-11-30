# Lab 5 实验报告：用户进程管理与 Copy-on-Write 机制实现及安全防御

## 1. 实验背景与目标

在操作系统中，进程创建（`fork`）是一个高频且开销较大的操作。传统的 `fork` 会完整复制父进程的内存空间给子进程，这不仅浪费内存，还导致创建速度缓慢。Copy-on-Write (COW) 技术通过让父子进程共享物理内存，仅在写入时才进行复制，极大地优化了性能。

本实验的主要目标是：
1.  理解并实现用户进程的生命周期管理（`fork`, `exec`, `wait`, `exit`）。
2.  在 ucore 中实现高效的 Copy-on-Write 机制。
3.  深入分析 COW 机制中潜在的竞态条件漏洞（Dirty COW），并设计防御方案。
4.  通过回归测试和 PoC 攻击测试，验证系统的功能正确性与安全性。

## 2. Copy-on-Write 机制原理

Copy-on-Write (COW) 是一种推迟内存复制的优化策略。其核心思想是：**共享读，写时复制**。

### 2.1 状态机模型
物理页面的状态转换可以用有限状态自动机（FSM）来描述：
1.  **Exclusive (Writable)**: 页面由单个进程独占，且可读写。
    *   `page_ref = 1`
    *   PTE 权限: `PTE_R | PTE_W`
2.  **Shared (Read-Only)**: 页面由多个进程共享（父子进程），只读。
    *   `page_ref > 1`
    *   PTE 权限: `PTE_R` (无 `PTE_W`)

### 2.2 转换事件
*   **Fork**: 父进程的 `Exclusive` 页面转变为 `Shared`，子进程映射同一物理页，双方 PTE 均设为只读。
*   **Write Fault**: 进程尝试写入 `Shared` 页面，触发缺页异常。内核分配新页，复制内容，将当前进程的 PTE 指向新页并设为 `Exclusive` (可写)。

## 3. 实验环境与基础代码分析

### 3.1 关键数据结构
*   `struct mm_struct`: 描述进程的虚拟内存空间，包含 `mmap_list` (VMA 链表) 和 `pgdir` (页目录)。
*   `struct vma_struct`: 描述一段连续的虚拟内存区域，包含 `vm_start`, `vm_end` 和 `vm_flags` (权限标志)。
*   `struct Page`: 描述物理页，包含 `ref` (引用计数)。

### 3.2 关键文件
*   `kern/mm/pmm.c`: 物理内存管理，包含 `copy_range` (内存复制)。
*   `kern/mm/vmm.c`: 虚拟内存管理，包含 `do_pgfault` (缺页处理)。
*   `kern/process/proc.c`: 进程管理，包含 `do_fork`, `do_exit` 等。

## 4. 练习2：父进程内存复制 (`copy_range`) 实现

`copy_range` 函数负责在 `fork` 时将父进程的内存空间复制给子进程。为了支持 COW，我们需要修改其逻辑。

### 4.1 实现逻辑
1.  **遍历页表**：遍历父进程指定地址范围内的页表项 (PTE)。
2.  **COW 判断**：检查 `share` 参数（由 `dup_mmap` 传入）。
    *   如果 `share == 0`：执行深拷贝（分配新页 -> `memcpy` -> 映射）。
    *   如果 `share == 1`：执行浅拷贝（COW）。
3.  **COW 操作细节**：
    *   获取父进程 PTE 指向的物理页 `page`。
    *   **映射子进程**：使用 `page_insert` 将该物理页映射到子进程页表。
    *   **权限设置**：关键在于**清除父子进程 PTE 的写权限 (`PTE_W`)**。这确保了后续的写操作会触发异常。
    *   **引用计数**：原子增加物理页的引用计数 (`page_ref_inc`)。

## 5. 练习3：进程创建与执行流程分析

### 5.1 `fork` 流程
1.  `alloc_proc`: 分配 PCB。
2.  `setup_kstack`: 分配内核栈。
3.  `copy_mm`: 复制内存空间（调用 `copy_range`，启用 COW）。
4.  `copy_thread`: 设置 TrapFrame，子进程 `a0 = 0`，`epc` 指向 `forkret`。
5.  `wakeup_proc`: 将子进程加入运行队列。

### 5.2 `exec` 流程
1.  `exit_mmap`: 清空当前进程内存。
2.  `load_icode`: 解析 ELF，建立新的内存映射。
3.  设置 TrapFrame，`epc` 指向 ELF 入口。

### 5.3 `wait` 与 `exit`
*   `exit`: 释放资源，变为 `ZOMBIE`，唤醒父进程。
*   `wait`: 查找 `ZOMBIE` 子进程，回收 PCB 和内核栈。

## 6. 缺页异常处理 (`do_pgfault`) 与 COW 核心逻辑

`do_pgfault` 是 COW 机制的执行者。当进程尝试写入只读的共享页面时，CPU 触发 `CAUSE_STORE_PAGE_FAULT`。

### 6.1 处理流程
1.  **查找 VMA**：根据出错地址查找对应的 `vma_struct`。
2.  **合法性检查**：
    *   如果地址无效或 VMA 不存在，报错。
    *   **关键安全检查**：如果试图写入 (`error_code & CAUSE_STORE_PAGE_FAULT`)，必须检查 `vma->vm_flags & VM_WRITE`。如果 VMA 本身不可写（如代码段），则这是非法访问，应直接终止进程（防御 Dirty COW 的关键）。
3.  **COW 触发条件**：
    *   是写操作。
    *   PTE 有效 (`PTE_V`) 但不可写 (`!PTE_W`)。
4.  **执行复制**：
    *   如果 `page_ref > 1`：说明有共享者。分配新页，`memcpy` 内容，映射新页为可写，旧页引用计数减一。
    *   如果 `page_ref == 1`：说明是最后一个持有者。直接将当前 PTE 设为可写即可（优化）。

## 7. Dirty COW 漏洞原理深度剖析

### 7.1 漏洞概述
Dirty COW (CVE-2016-5195) 是 Linux 内核历史上最严重的提权漏洞之一。它利用了内核在处理 COW 时的竞态条件，允许攻击者写入只读内存映射（如 `/etc/passwd` 的映射）。

### 7.2 漏洞成因
在 Linux 中，COW 操作涉及多个步骤（解锁 -> 复制 -> 加锁 -> 替换 PTE）。攻击者利用 `madvise(MADV_DONTNEED)` 在这些步骤之间清空页表，导致内核在重试时错误地直接写入了原始的物理页（即磁盘上的只读文件），而不是写入新分配的副本。

### 7.3 ucore 中的潜在风险
虽然 ucore 没有 `madvise`，但存在类似的逻辑漏洞：
1.  **权限检查缺失**：如果 `do_pgfault` 忽略了 VMA 的权限检查，直接为只读 VMA 执行 COW，攻击者就能获得代码段的写权限。
2.  **并发锁竞争**：`fork` 过程持有 `mm_lock`，如果此时发生缺页且处理不当，可能导致死锁或状态不一致。

## 8. 漏洞复现与 PoC 测试套件设计

为了验证系统的安全性，我们设计了 `user/poc_suite.c` 测试套件。

### 8.1 测试用例设计
1.  **Code Write (代码段写入)**:
    *   原理：获取 `main` 函数地址，尝试写入。
    *   预期：内核应检测到 VMA 为只读，杀死进程。
2.  **Rodata Write (只读数据写入)**:
    *   原理：尝试写入 `const` 字符串。
    *   预期：内核杀死进程。
3.  **Kernel Write (内核空间写入)**:
    *   原理：尝试写入 `KERNBASE` 以上地址。
    *   预期：内核杀死进程。
4.  **Fork Race (并发压力测试)**:
    *   原理：创建 20 个子进程，同时写入共享的大数组。
    *   预期：所有子进程正确触发 COW，数据互不干扰，无内核崩溃。

## 9. 防御方案设计与实现

我们实施了三层防御体系，确保系统的安全性与稳定性。

### 9.1 第一层：严格的 VMA 权限检查
在 `do_pgfault` 入口处增加检查：
```c
if (write && !(vma->vm_flags & VM_WRITE)) {
    cprintf("do_pgfault failed: write fault, but vma not writable\n");
    goto failed;
}
```
这直接阻断了所有针对只读映射的非法 COW 尝试。

### 9.2 第二层：原子引用计数
将 `page_ref_inc/dec` 修改为使用 GCC 原子内置函数 `__sync_add_and_fetch`。这防止了在多核环境下，多个进程同时 `fork` 或缺页导致引用计数损坏（例如两个进程同时读到 `ref=2`，都以为自己不是最后一个，导致内存泄漏或过早释放）。

### 9.3 第三层：临界区保护与锁机制优化
在回归测试中，我们发现 `forktest`（高并发）和 `spin`（死循环）测试存在冲突。
*   **问题**：`fork` 持有 `mm_lock` 时若被抢占，其他进程（或子进程）缺页时尝试获取锁会失败。如果自旋等待，会导致死锁（单核）；如果调度，会导致 `forktest` 性能急剧下降。
*   **解决方案**：在 `kern/process/proc.c` 的 `copy_mm` 中，**关中断**保护临界区。
```c
local_intr_save(intr_flag);
lock_mm(oldmm);
dup_mmap(mm, oldmm);
unlock_mm(oldmm);
local_intr_restore(intr_flag);
```
这确保了 `fork` 操作原子执行，不会被抢占。因此 `do_pgfault` 中的 `try_lock` 总是能快速成功，无需复杂的调度逻辑。

## 10. 实验结果验证与总结

### 10.1 标准回归测试 (`make grade`)
*   **Spin Test**: 通过 (34.2s)。说明锁机制没有导致死锁或饥饿。
*   **Forktest**: 通过 (1.7s)。说明 COW 机制性能良好，无额外调度开销。
*   **总分**: 130/130。

### 10.2 安全性测试 (`run_poc_suite.sh`)
*   **Code Write**: PASS (Process Killed)。防御成功。
*   **Rodata Write**: PASS (Process Killed)。防御成功。
*   **Kernel Write**: PASS (Process Killed)。防御成功。
*   **Fork Race**: PASS。并发 COW 逻辑正确。

### 10.3 总结
本实验成功在 ucore 中实现了 Copy-on-Write 机制，显著提升了进程创建效率。通过深入分析 Dirty COW 漏洞，我们设计并实施了包含权限检查、原子操作和锁优化的多层防御方案。最终的测试结果表明，该方案既保证了系统的安全性，又维持了高性能和稳定性，完美解决了实验中遇到的并发回归问题。
