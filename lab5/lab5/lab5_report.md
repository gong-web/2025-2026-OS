# Lab 5 实验报告

## 练习2: 父进程复制自己的内存空间给子进程

### 设计实现过程

`copy_range` 函数的主要任务是将父进程的内存空间复制给子进程。在 `kern/mm/pmm.c` 中实现。

1.  **基本实现 (非COW)**:
    *   遍历父进程的页表，找到有效的 PTE。
    *   获取父进程页面的内核虚拟地址 (`src_kvaddr`)。
    *   为子进程分配一个新的物理页 (`npage`)，并获取其内核虚拟地址 (`dst_kvaddr`)。
    *   使用 `memcpy` 将父进程页面的内容复制到子进程的新页面。
    *   使用 `page_insert` 将新页面映射到子进程的页表中，权限与父进程相同。

2.  **Copy on Write (COW) 实现**:
    *   如果 `share` 标志为真（在 `dup_mmap` 中设置），则不立即复制内存。
    *   将父进程的页面映射到子进程的页表中。
    *   **关键点**: 将父进程和子进程的 PTE 权限都设置为 **只读** (移除 `PTE_W`)。
    *   增加页面的引用计数。
    *   当任一进程尝试写入该页面时，会触发 Page Fault。

### Copy on Write 机制设计

**概要设计**:
COW 是一种推迟内存复制的优化策略。只有当进程尝试写入共享页面时，才真正进行复制。

1.  **Fork 时**:
    *   `do_fork` -> `copy_mm` -> `dup_mmap` -> `copy_range`。
    *   在 `copy_range` 中，不分配新页面，而是将子进程的 PTE 指向父进程的物理页。
    *   将父子进程的该页面 PTE 均设为只读 (`PTE_W` 置 0)。
    *   增加物理页的引用计数。

2.  **Write Fault 时**:
    *   CPU 触发 `CAUSE_STORE_PAGE_FAULT` 异常。
    *   进入 `trap` -> `exception_handler` -> `do_pgfault`。
    *   `do_pgfault` 检测到是写异常，且 VMA 标记为可写 (`VM_WRITE`)，但 PTE 为只读。
    *   分配一个新的物理页。
    *   将原页面的内容复制到新页面。
    *   更新当前进程的 PTE，指向新页面，并设置为 **可写** (`PTE_W` 置 1)。
    *   减少原页面的引用计数。

## 练习3: 阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现

### fork/exec/wait/exit 执行流程分析

1.  **fork**:
    *   **执行流程**: `do_fork` 是核心。
        *   分配并初始化进程控制块 (`alloc_proc`)。
        *   分配并初始化内核栈 (`setup_kstack`)。
        *   复制内存空间 (`copy_mm`) -> 这里涉及 COW。
        *   复制上下文 (`copy_thread`)，设置 `tf->a0 = 0` (子进程返回值为0)。
        *   将新进程加入进程列表，设为 `PROC_RUNNABLE`。
    *   **内核态/用户态**: `fork` 是系统调用，用户态调用 `ecall` 进入内核态执行 `do_fork`。执行完毕后，父进程返回子进程 PID，子进程返回 0，通过 `sret` 返回用户态。

2.  **exec**:
    *   **执行流程**: `do_execve`。
        *   回收当前进程的内存空间 (`exit_mmap`)。
        *   加载新的可执行文件 (ELF 解析)。
        *   建立新的内存映射。
        *   设置新的栈和 trapframe (修改 `epc` 为入口地址)。
    *   **内核态/用户态**: 用户态调用 `exec`，内核态完成替换，返回用户态时从新程序入口开始执行。

3.  **wait**:
    *   **执行流程**: `do_wait`。
        *   查找状态为 `PROC_ZOMBIE` 的子进程。
        *   如果找到，回收子进程剩余资源 (PCB, 内核栈)，返回子进程 PID 和退出码。
        *   如果子进程还在运行，当前进程进入 `PROC_SLEEPING` 状态，等待被唤醒。
    *   **内核态/用户态**: 用户态调用 `wait`，内核态挂起或返回结果。

4.  **exit**:
    *   **执行流程**: `do_exit`。
        *   回收内存空间 (`exit_mmap`)。
        *   设置状态为 `PROC_ZOMBIE`。
        *   唤醒父进程 (如果父进程在 `wait`)。
        *   主动调度 (`schedule`)。
    *   **内核态/用户态**: 用户态调用 `exit`，内核态清理资源，不再返回该进程的用户态。

### 用户态进程生命周期图

```
       +--> [NEW]
       |      |
       |    fork()
       |      v
       |  [RUNNABLE] <--------+
       |      |               |
       |   schedule()         |
       |      v               |
       |  [RUNNING]  ---------+
       |      |    \
       |   exit()   \ wait() / event
       |      |      \
       |      v       v
       +-- [ZOMBIE] [SLEEPING]
```

## 扩展练习 Challenge: Copy on Write (COW)

### 实现源码

(见 `kern/mm/pmm.c` 中的 `copy_range` 和 `kern/mm/vmm.c` 中的 `do_pgfault`)

### 设计报告

在 `ucore` 中实现 COW 的关键在于利用页表项的权限位和页面的引用计数。

1.  **修改 `dup_mmap`**: 将 `share` 参数设为 1，启用共享。
2.  **修改 `copy_range`**:
    *   当 `share=1` 时，不进行 `memcpy`。
    *   使用 `page_insert` 映射同一物理页。
    *   清除 `PTE_W` 位，使页面只读。
3.  **实现 `do_pgfault`**:
    *   捕获 `CAUSE_STORE_PAGE_FAULT`。
    *   检查 VMA 权限是否允许写入。
    *   检查 PTE 是否有效且只读。
    *   如果是 COW 页面（引用计数 > 1），则分配新页，复制内容，重新映射为可写。
    *   如果是最后一个引用者（引用计数 == 1），直接修改 PTE 为可写即可（优化）。

### Dirty COW 漏洞分析

Dirty COW (CVE-2016-5195) 是 Linux 内核的一个竞争条件漏洞。
在 COW 过程中，内核需要经过 (1) 检查页面是否私有 (2) 解锁 (3) 复制页面 (4) 重新加锁 (5) 替换页表项 等步骤。
如果在这个过程中，另一个线程通过 `madvise(MADV_DONTNEED)` 丢弃了页面，可能导致写操作直接作用于原始的只读文件映射上，从而修改了只读文件（如 `/etc/passwd`）。

在 ucore 的简单实现中，由于内核是不可抢占的（或者说我们在处理 page fault 时没有复杂的锁机制和并发处理），且没有多线程支持（只有多进程），因此很难直接复现 Dirty COW 漏洞。但在多核或支持内核抢占的系统中，必须确保 COW 操作的原子性，或者在操作期间持有适当的锁。
