# COW（Copy-on-Write）实现与测试说明（Lab5 / uCore-RISC-V）

本文档说明三件事：
1) 我们的 COW 在内核里是如何实现的（fork 时怎么共享、写时怎么复制）。
2) COW 是如何被测试的（官方 grade + 两个样例程序）。
3) 自定义脚本 `tools/cow_grade.sh` 每一段在做什么、为什么这样写。

> 说明：以下描述以当前仓库 `lab5/lab5` 目录下代码为准。

---

## 1. COW 要解决什么问题

`fork()` 的语义是：子进程在创建瞬间拥有父进程地址空间的“逻辑副本”。

传统做法是 fork 时把父进程的用户页全部复制一份给子进程，简单但代价很高。

COW 的核心思想是：
- fork 时**先不复制物理页**，父子进程先**共享同一份物理页**；
- 共享期间把这些页都设置成“只读（不可写）”；
- 当某个进程第一次尝试写共享页时，触发**缺页异常（store page fault）**；
- 内核在缺页异常里为该进程分配新页、拷贝旧页内容，然后把该虚拟地址重新映射到新页并恢复写权限。

这样大多数 fork+exec 的场景几乎不需要复制大量内存。

---

## 2. 代码实现总览（从 fork 到写时复制）

实现链路可以概括为两段：

1) **fork 时：建立“共享 + 只读 + 标记 COW”的映射**
- 入口：`do_fork()`（进程创建）
- 路径：`copy_mm()` → `dup_mmap()` → `copy_range()`

2) **写共享页时：在 `do_pgfault()` 中完成复制并恢复可写**
- 入口：trap 里收到 `CAUSE_STORE_PAGE_FAULT`
- 路径：`exception_handler()` → `do_pgfault()`

下面按关键文件说明。

---

## 3. 关键内核文件与关键逻辑

### 3.1 fork 复制 VMA：`kern/mm/vmm.c` 的 `dup_mmap`

位置：`dup_mmap(struct mm_struct *to, struct mm_struct *from)`

做的事情：
- 遍历父进程 `from->mmap_list` 中每个 VMA
- 在子进程 `to` 中创建一个同样范围/权限的 VMA
- 调用 `copy_range(to->pgdir, from->pgdir, vma->vm_start, vma->vm_end, share)`

当前实现里：
- `bool share = 1;`，表示 fork 走“共享页表映射”的分支（由 `copy_range` 负责把可写页变成 COW）。

这一步的关键点是：**VMA（逻辑区域）一定要复制**，但**物理页不直接复制**。

### 3.2 页表复制/共享：`kern/mm/pmm.c` 的 `copy_range`

位置：`copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share)`

逻辑（按页遍历虚拟地址区间）：
- 找到父进程该地址的 PTE（页表项）
- 如果该页有效（`PTE_V`），就在子进程页表里建立映射

当 `share == 1` 时：
- 读取原 PTE 的权限位，得到 `perm`
- 对“原本可写”的页：
  - 清掉 `PTE_W`
  - 加上软件保留位里的 `PTE_COW`
  - **同时更新父进程与子进程**对该页的映射（父子都变成“只读 + COW 标记”）
- 对“原本不可写”的页（例如纯代码页/只读数据页）：
  - 直接共享，保持原权限（尤其避免破坏可执行权限）

当 `share == 0` 时：
- 走深拷贝：分配新页、`memcpy` 整页内容、再映射到子进程。

额外细节：
- 这里还会保留/携带 `PTE_A/PTE_D`（Accessed/Dirty）位，原因是某些 QEMU/软件管理 A/D 位的组合下，如果丢了 A 位，可能会出现重复 fault（实现里选择显式保留）。

### 3.3 写时复制缺页处理：`kern/mm/vmm.c` 的 `do_pgfault`

位置：`int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)`

流程（重点看 store page fault）：
1) `find_vma(mm, addr)`：确认地址属于某个 VMA
2) 判断是否写异常：`write = (error_code == CAUSE_STORE_PAGE_FAULT)`
3) **权限安全检查（Dirty-COW 防护点）**：
   - 如果是写异常但 `vma->vm_flags` 不包含 `VM_WRITE`，直接失败
   - 这避免了“利用 COW/页故障绕过 VMA 权限”的情况
4) 计算要映射的 `perm`（根据 VMA 的 VM_READ/VM_WRITE/VM_EXEC 设置 PTE_R/W/X/U），并显式带上 `PTE_A`，对可写页也带 `PTE_D`
5) 获取/创建 PTE：`get_pte(mm->pgdir, addr, 1)`
6) 如果 `*ptep == 0`：按需分配新页并映射（正常的 demand paging）
7) 否则，如果是写异常且 PTE 当前不可写：进入 COW 分支
   - `page = pte2page(*ptep)`
   - 若 `page_ref(page) > 1`：
     - 分配新页 `npage`
     - `memcpy` 旧页内容到新页
     - `page_insert(mm->pgdir, npage, addr, perm)` 建立新映射（此时恢复写权限）
   - 若 `page_ref(page) == 1`：
     - 说明该页其实没有被别的进程共享（或者已经变成独占）
     - 直接 `page_insert(mm->pgdir, page, addr, perm)` 把页改成可写即可

> 备注：严格意义上，COW 分支通常还会检查软件位 `PTE_COW`，以区分“真正的 COW 只读页”和“其他原因导致的只读页”。当前实现的触发条件是“写异常 + PTE 有效 + 不可写”，并依赖前面的 VMA 写权限检查来保证语义正确。

---

## 4. COW 的测试怎么做

测试分两类：

### 4.1 官方 `make grade`（覆盖面更广）

在 WSL 下使用脚本运行（它会先激活 RISC-V 工具链环境）：
- 脚本：`run_grade_wsl.sh`
- 做的事：`make clean` → `make` → `make grade`

`make grade` 会依次运行一组用户程序（如 `badarg`、`spin`、`forktest` 等），它们能覆盖：
- fork 正常性
- 进程调度/yield
- kill/waitpid 参数检查
- 异常处理（faultread/divzero…）

这类测试的特点：
- 不直接“宣称你实现了 COW”，但会用各种 corner case 把 fork/mm/page fault 的实现压一遍。

### 4.2 两个“专门验证 COW 的样例程序”

这两个程序位于：
- `user/cow_test.c`
- `user/dirtycow_defense_test.c`

它们关注点是：
1) `cow_test`：验证“共享 → 写触发复制 → 父子数据隔离”
   - 关键输出（供脚本匹配）：
     - `Child: PASSED - COW triggered successfully`
     - `Parent: PASSED - Data isolation successful`

2) `dirtycow_defense_test`：验证“写权限检查 + COW 隔离”
   - 关键输出（供脚本匹配）：
     - `Test 2: Verify permission check during COW`
     - `Parent: PASSED - Data unchanged`

这两个程序不依赖 `malloc`，主要通过全局/静态变量进行读写验证，避免用户态库缺失导致的链接问题。

---

## 5. 自定义脚本 `tools/cow_grade.sh` 解释

这个脚本的目标：
- 一键编译并运行 `cow_test` 和 `dirtycow_defense_test`
- 通过匹配 QEMU 串口输出里的“关键行”来判定是否通过
- 避免 QEMU/GDB 卡住：默认不启用 GDB，采用“运行固定秒数后 kill”的方式收集日志

### 5.1 用法

- 普通运行（安静模式）：
  - `bash tools/cow_grade.sh`
- 详细输出（看到编译与匹配细节）：
  - `bash tools/cow_grade.sh -v`

可选环境变量：
- `QEMU_RUN_SECS=15`：无 GDB 模式下，QEMU 运行多少秒后终止（默认 15）。
- `COW_GRADE_USE_GDB=1`：改回“用 GDB 下断点驱动运行”的模式（不推荐，容易因连接/断点导致卡死）。

### 5.2 脚本结构（从上到下）

1) **`-v` 参数处理**
- `-v`：把 stdout/stderr 指向终端，便于看编译命令与调试信息
- 非 `-v`：输出重定向到 `/dev/null`，保持安静

2) **自动激活工具链（WSL 友好）**
- 如果 `riscv64-unknown-elf-gcc` 不在 PATH 里，脚本会尝试 `source`：
  - `/mnt/d/.../activate_riscv_env.sh`（你们当前 WSL 的工具链激活脚本）
  - `labcode/env/activate_os_env.sh`（仓库内的备用激活脚本）
- 如果仍找不到 gcc，就直接报错并提示先跑 `run_grade_wsl.sh`

3) **`make_print` 获取 Makefile 里的工具路径**
- 通过 `make print-xxx` 拿到 `qemu`、`GRADE_QEMU_OUT`、`GDB` 等变量
- 这样不需要在脚本里硬编码 qemu 路径

4) **`run_qemu`：启动 QEMU、收集日志、再终止**
- QEMU 会用 `-serial file:$qemu_out` 把串口输出写到文件（这是后续 grep 的输入）
- 默认不启用 GDB：
  - `sleep $qemu_run_secs`
  - `kill $pid`
- 如果启用 GDB：会连上 gdbserver 设置断点并 `continue`

5) **`run_test`：一次完整测试流程**
- 负责：设置 tag/prog、构建目标（`build-$prog`）、运行 QEMU、最后检查输出

6) **检查函数 `check_regexps/default_check`**
- 本脚本的 check 方式是 `grep`：
  - 要求日志中包含指定的字符串行（缺失则 FAIL）
- `default_check` 会把 `pts` 设成 7，所以两条测试各 7 分，最后总分 14/14。

7) **底部两条用例定义**
- `run_test -prog 'cow_test' ...`：检查 3 条关键输出
- `run_test -prog 'dirtycow_defense_test' ...`：检查 3 条关键输出

---

## 6. 如何复现（建议命令）

在 Windows PowerShell 里运行 WSL 命令即可：

1) 官方满分验证：
- `wsl -e bash -lc "cd /mnt/d/gds/Documents/2025-2026-OS/lab5/lab5; bash run_grade_wsl.sh"`

2) 两样例验证：
- `wsl -e bash -lc "cd /mnt/d/gds/Documents/2025-2026-OS/lab5/lab5; bash tools/cow_grade.sh -v"`

---

## 7. 你可能会问的两个常见问题

1) 为什么脚本要“运行几秒就 kill QEMU”？
- 因为这类测试只是为了抓到串口输出的关键行；很多用户程序不会主动退出或会进入等待/死循环（例如 `spin`），让 QEMU 自己退出不现实。
- 所以脚本用固定时间窗口收集日志，是最稳的自动化方式。

2) 为什么要在 `do_pgfault` 里做 VMA 写权限检查？
- 这是为了保证：即使攻击者/bug 让一个只读区域的 PTE 带了 COW 行为，也不能通过“写触发缺页 → 内核帮你复制并给你写权限”绕过 VMA 的权限设计。

---

如需我把本文档进一步“对照代码行”标注到更细（比如把每一步对应到具体函数/分支的伪代码），我也可以继续补充。