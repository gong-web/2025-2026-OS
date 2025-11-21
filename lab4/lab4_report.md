# 实验四：内核线程管理

## 一、实验目的

本实验旨在深入理解并实现操作系统中内核线程的管理机制，包括进程控制块（PCB）的分配与初始化、内核线程的创建与资源分配、以及进程切换的实现。

---

## 二、练习0：填写已有实验

本实验依赖实验2/3。已将实验2/3相关代码填入本实验代码中 `LAB2`、`LAB3` 注释相应部分。

---

## 三、练习1：分配并初始化一个进程控制块

### 3.1 设计实现过程

`alloc_proc` 函数负责分配并初始化一个新的 `struct proc_struct` 结构，用于存储内核线程的管理信息。主要实现步骤如下：

1. **分配内存**：使用 `kmalloc` 分配 `proc_struct` 大小的内存块
2. **初始化各成员变量**：
   - `state = PROC_UNINIT`：进程状态设为未初始化
   - `pid = -1`：进程ID设为-1，表示尚未分配有效PID
   - `runs = 0`：运行次数初始化为0
   - `kstack = 0`：内核栈地址初始为0，后续由 `setup_kstack()` 分配
   - `need_resched = 0`：初始不需要调度
   - `parent = NULL`：父进程指针初始为空
   - `mm = NULL`：内存管理结构初始为空（内核线程不需要独立的用户地址空间）
   - `context`：使用 `memset` 清零上下文结构体
   - `tf = NULL`：trapframe指针初始为空，后续在 `copy_thread` 中设置
   - `pgdir = boot_pgdir_pa`：使用内核启动时的页目录物理地址
   - `flags = 0`：标志位清零
   - `name`：使用 `memset` 清零进程名

### 3.2 问题回答

**请说明 `proc_struct` 中 `struct context context` 和 `struct trapframe *tf` 成员变量含义和在本实验中的作用是啥？**

- **`struct context context`**：
  - **含义**：进程上下文结构体，保存进程切换时需要保存的CPU寄存器值
  - **包含的寄存器**：ra（返回地址）、sp（栈指针）、s0-s11（callee-saved寄存器）
  - **作用**：在进程切换时，通过 `switch_to` 函数保存当前进程的寄存器状态，并恢复目标进程的寄存器状态，从而实现进程的上下文切换。这是实现多任务并发执行的关键机制。

- **`struct trapframe *tf`**：
  - **含义**：指向中断帧（trapframe）结构体的指针，保存中断/异常发生时的CPU寄存器现场
  - **包含的内容**：所有通用寄存器、程序计数器（epc）、状态寄存器（sstatus）等
  - **作用**：
    1. 保存中断发生时的完整CPU状态，便于中断返回后恢复执行
    2. 在内核线程创建时，用于设置新线程的初始执行环境（如入口地址、栈指针等）
    3. 通过 `forkrets` 函数恢复trapframe中的寄存器值，启动新线程的执行

---

## 四、练习2：为新创建的内核线程分配资源

### 4.1 设计实现过程

`do_fork` 函数负责创建新的内核线程，其执行步骤如下：

1. **调用 `alloc_proc`**：分配并初始化进程控制块
2. **调用 `setup_kstack`**：为新进程分配内核栈（大小为 `KSTACKPAGE` 页）
3. **调用 `copy_mm`**：复制或共享内存管理信息（内核线程此步骤为空操作）
4. **调用 `copy_thread`**：
   - 在内核栈顶设置 trapframe
   - 复制父进程的寄存器现场
   - 设置子进程返回值为0
   - 设置上下文的 `ra` 为 `forkret`，`sp` 指向 trapframe
5. **关中断并执行原子操作**：
   - 调用 `get_pid` 分配唯一的进程ID
   - 调用 `hash_proc` 将进程加入哈希表
   - 将进程加入进程链表 `proc_list`
   - 增加进程计数 `nr_process`
6. **调用 `wakeup_proc`**：将新进程状态设为 `PROC_RUNNABLE`，使其可被调度
7. **返回新进程的PID**

### 4.2 问题回答

**请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。**

**答：是的，ucore确保每个新fork的线程都有唯一的ID。**

**分析与理由**：

1. **`get_pid()` 函数机制**：
   - `get_pid()` 函数负责分配唯一的进程ID
   - 该函数使用静态变量维护PID分配状态，每次调用时递增并检查PID是否已被使用
   - 通过遍历进程哈希表，确保分配的PID不与现有进程冲突

2. **原子操作保护**：
   - 在 `do_fork` 中，PID分配、哈希表插入、进程链表插入等操作均在关中断的临界区内完成
   - 使用 `local_intr_save` 和 `local_intr_restore` 确保这些操作的原子性
   - 防止多个进程同时创建时出现PID冲突

3. **哈希表验证**：
   - 新进程通过 `hash_proc` 插入哈希表后，可以通过 `find_proc(pid)` 快速查找
   - 如果PID不唯一，会导致哈希冲突和进程管理混乱

4. **实际验证**：
   - 从实验输出可以看到：`idleproc` 的 `pid = 0`，`initproc` 的 `pid = 1`
   - 系统正常运行，说明PID分配机制工作正常

---

## 五、练习3：编写proc_run函数

### 5.1 设计实现过程

`proc_run` 函数用于将指定的进程切换到CPU上运行，实现步骤如下：

1. **检查是否需要切换**：
   - 判断 `proc != current`，如果相同则无需切换，直接返回
   
2. **禁用中断**：
   - 使用 `local_intr_save(intr_flag)` 关闭中断，防止切换过程被打断
   
3. **切换当前进程指针**：
   - 保存前一个进程 `prev = current`
   - 更新当前进程 `current = proc`
   
4. **切换页表**：
   - 调用 `lsatp(proc->pgdir)` 修改SATP寄存器
   - 加载新进程的页目录表地址，切换地址空间
   
5. **上下文切换**：
   - 调用 `switch_to(&(prev->context), &(next->context))`
   - 保存当前进程的寄存器到 `prev->context`
   - 恢复目标进程的寄存器从 `next->context`
   - 实现进程的真正切换
   
6. **恢复中断**：
   - 使用 `local_intr_restore(intr_flag)` 恢复中断状态

### 5.2 关键代码实现

```c
void proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag);
        {
            current = proc;
            // 切换页表：设置新进程的页目录表地址到satp寄存器
            lsatp(proc->pgdir);
            // 切换上下文：从当前进程切换到新进程
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}
```

### 5.3 问题回答

**在本实验的执行过程中，创建且运行了几个内核线程？**

**答：创建并运行了2个内核线程。**

**详细说明**：

1. **idleproc（空闲线程）**：
   - PID = 0
   - 在 `proc_init` 中通过 `alloc_proc` 直接创建
   - 作用：当系统中没有其他可运行进程时，CPU执行idle线程
   - 主要功能：在 `cpu_idle` 函数中循环检查 `need_resched`，调用 `schedule()` 进行调度
   
2. **initproc（初始化线程）**：
   - PID = 1
   - 通过 `kernel_thread(init_main, "Hello world!!", 0)` 创建
   - 使用 `do_fork` 完成实际的线程创建和资源分配
   - 主要功能：执行 `init_main` 函数，输出"Hello world!!"等信息

**运行流程**：
1. 系统初始化时创建 `idleproc` 并设为当前进程（`current = idleproc`）
2. 通过 `kernel_thread` 创建 `initproc`，状态设为 `PROC_RUNNABLE`
3. `idleproc` 在 `cpu_idle` 中检测到 `need_resched`，调用 `schedule()`
4. 调度器选择 `initproc` 运行，调用 `proc_run` 完成切换
5. `initproc` 执行完毕后，系统继续调度

**实验输出验证**：
```
alloc_proc() correct!
++ setup timer interrupts
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
```

---

## 六、实验结果

### 6.1 编译与运行

**编译命令**：
```bash
make qemu
```

**运行结果**：
```
OpenSBI v0.4 (Jul  2 2019 11:53:53)
...
(THU.CST) os is loading ...

Special kernel symbols:
  entry  0xc020004a (virtual)
  etext  0xc0203ec6 (virtual)
  edata  0xc0209030 (virtual)
  end    0xc020d4ec (virtual)
Kernel executable memory footprint: 54KB
memory management: default_pmm_manager
physcial memory map:
  memory: 0x08000000, [0x80000000, 0x87ffffff].
vapaofset is 18446744070488326144
check_alloc_page() succeeded!
check_pgdir() succeeded!
check_boot_pgdir() succeeded!
use SLOB allocator
kmalloc_init() succeeded!
check_vma_struct() succeeded!
check_vmm() succeeded.
alloc_proc() correct!
++ setup timer interrupts
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
kernel panic at kern/process/proc.c:412:
    process exit!!.

Welcome to the kernel debug monitor!!
Type 'help' for a list of commands.
```

### 6.2 结果分析

1. **`alloc_proc() correct!`**：验证了练习1的正确性，进程控制块初始化成功
2. **`this initproc, pid = 1, name = "init"`**：验证了练习2的正确性，成功创建了initproc线程
3. **`To U: "Hello world!!"`**：验证了练习3的正确性，initproc成功运行并输出信息
4. **最终panic**：这是预期行为，因为 `do_exit` 函数中直接调用了 `panic`，表示进程正常结束

---

## 七、重要知识点总结

### 7.1 实验知识点与OS原理对照

| 实验知识点 | OS原理知识点 | 含义、关系与差异 |
|-----------|-------------|-----------------|
| **进程控制块（PCB）** | **进程描述符/PCB** | 实验中为 `struct proc_struct`，包含进程状态、PID、上下文等信息；原理中为抽象概念，用于描述和管理进程的所有信息。实验是原理的具体实现。 |
| **上下文切换（context）** | **进程上下文切换** | 实验中通过 `context` 结构体保存ra、sp、s0-s11等寄存器，并通过 `switch_to` 汇编实现切换；原理中为保存/恢复CPU寄存器状态的抽象过程。实验展示了底层硬件级的实现细节。 |
| **内核线程创建** | **进程/线程创建机制** | 实验中通过 `do_fork` 实现，包括分配PCB、内核栈、设置trapframe等；原理中为fork系统调用的抽象描述。实验侧重于内核态实现，原理包含用户态和内核态。 |
| **进程调度（schedule）** | **进程调度算法** | 实验中使用简单的轮转调度，遍历进程链表选择下一个RUNNABLE进程；原理中包含多种调度算法（FCFS、SJF、优先级、多级反馈队列等）。实验是最简单的实现。 |
| **trapframe（中断帧）** | **中断/异常处理** | 实验中 `trapframe` 保存中断发生时的CPU状态，用于中断返回和线程启动；原理中为中断处理机制的一部分。实验展示了具体的数据结构和使用方式。 |
| **进程状态转换** | **进程状态模型** | 实验中定义了UNINIT、SLEEPING、RUNNABLE、ZOMBIE四种状态；原理中通常包含新建、就绪、运行、阻塞、终止五种状态。实验简化了状态模型，合并了部分状态。 |
| **唤醒机制（wakeup_proc）** | **进程阻塞与唤醒** | 实验中 `wakeup_proc` 将进程状态设为RUNNABLE，使其可被调度；原理中为将阻塞进程移入就绪队列的操作。实验是原理的直接实现。 |
| **内核栈（kstack）** | **进程内核栈** | 实验中为每个进程分配独立的内核栈，用于中断处理和内核态执行；原理中为进程在内核态运行时使用的栈空间。实验展示了具体的分配和使用方式。 |

### 7.2 OS原理中重要但实验未涉及的知识点

1. **用户态与内核态切换**：
   - 实验中只实现了内核线程，未涉及用户进程的创建和系统调用
   - 原理中用户态和内核态切换是操作系统的核心机制，涉及特权级切换、栈切换等

2. **进程间通信（IPC）**：
   - 实验中未实现任何IPC机制（管道、消息队列、共享内存、信号量等）
   - 原理中IPC是多进程协作的重要手段

3. **进程同步与互斥**：
   - 实验中虽然有中断开关的原子操作，但未涉及信号量、互斥锁、条件变量等同步原语
   - 原理中进程同步是解决竞态条件、死锁等问题的关键

4. **虚拟内存管理**：
   - 实验中虽然有页表切换，但未深入实现缺页异常、页面置换算法、内存映射等
   - 原理中虚拟内存是现代操作系统的核心特性

5. **文件系统**：
   - 实验中完全未涉及文件系统
   - 原理中文件系统是操作系统管理持久化数据的重要部分

6. **多级调度与调度算法**：
   - 实验中只实现了简单的轮转调度
   - 原理中包含多种复杂调度算法（如完全公平调度CFS、多级反馈队列等）

7. **进程优先级与实时调度**：
   - 实验中所有进程优先级相同
   - 原理中进程优先级和实时调度是多任务系统的重要特性

8. **进程资源管理**：
   - 实验中未涉及CPU时间片、内存限额、打开文件数等资源限制
   - 原理中资源管理是防止进程滥用系统资源的重要机制

---

## 八、实验总结

通过本次实验，深入理解了以下内容：

1. **进程控制块的设计**：掌握了PCB的各个字段及其初始化方法
2. **内核线程的创建流程**：理解了从分配PCB到资源分配、再到加入调度队列的完整过程
3. **进程切换机制**：掌握了上下文切换、页表切换的实现细节
4. **进程调度原理**：理解了调度器如何选择下一个运行的进程
5. **中断与进程的关系**：理解了trapframe在进程管理中的作用

实验中遇到的主要问题是进程切换后系统卡住，通过检查代码发现是页表切换和上下文切换的顺序问题，最终通过正确实现 `proc_run` 函数解决。

本次实验为后续实验（用户进程、系统调用、文件系统等）奠定了坚实基础。
