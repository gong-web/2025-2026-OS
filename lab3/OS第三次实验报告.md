- 2312325 巩岱松
- 2311561 梁朝阳
- 2312145 郭子涵

# 练习一：中断与中断处理流程

## 实验概述

- **实验主题**：在最小可执行内核基础上，加入对中断与异常（trap）的完整支持，重点实现断点与时钟中断处理，通过周期性时钟事件验证中断系统的正确性。
- **前置依赖**：完成物理内存管理与页表机制；OpenSBI 提供 M 模式固件服务；QEMU 模拟器环境。
- **主要产物**：
  
  完整可运行的 S 模式 trap 框架（入口 `kern/trap/trapentry.S` + 分发与处理 `kern/trap/trap.c`）。
  
  时钟中断初始化与处理（`kern/driver/clock.c`）。
  
  中断开关接口（`kern/driver/intr.c`）与关键寄存器配置。
  
  中文实验报告（本文件），系统性阐述原理、实现、验证与扩展。

## 学习与掌握

- **RISC-V trap 基础**：中断/异常分类、委托（mideleg/medeleg）、关键 CSR（stvec/sepc/scause/stval/sstatus/sie）。
- **上下文管理**：按约定序布局保存/恢复 32 个通用寄存器 + CSR，`trapframe` 的 ABI 约束与栈帧组织。
- **中断路径**：stvec Direct 模式统一入口，`__alltraps → trap() → dispatch → handler → __trapret → sret`。
- **设备中断**：通过 OpenSBI 定时器服务与 `rdtime` 驱动周期性时钟中断。
- **内核原子性**：在关键区使用 `intr_disable/intr_enable` 封装的本地中断屏蔽/恢复。

## RISC-V 中断与异常综述

### 概念与动机

- **中断（Interrupt）**：外设异步发起的事件（如定时器到期、串口输入），打断当前执行流，优先处理外设服务。
- **异常（Exception）**：指令执行过程中同步产生（如非法指令、缺页、除零）。
- **陷入（Trap 指令性）**：主动触发（`ecall/ebreak`），用于系统调用或调试。
- RISC-V 统称为 **trap**，通过硬件保存最小上下文后，切换到内核特权级执行处理逻辑。

### 特权级与委托

- 特权级：`U < S < M`。默认 trap 进入 M，但通过 `mideleg/medeleg` 可委托到 S。
- 在本实验体系下，OpenSBI 启动时已将与 OS 相关的大多数 trap 委托至 S，内核在 S 处理：
  - 用户态异常（如 `ecall from U`、页错误）。
  - S 态外设中断（如定时器）。
  - S 态主动 `ecall` 仍会进入 M，由固件服务后 `mret` 返回。

### 关键 CSR 速览

- `stvec`：S 态 trap 向量。Direct 模式下指向唯一入口（对齐要求）。
- `sepc`：保存被打断/异常指令地址；返回时 `sret` 赋 `pc ← sepc`。
- `scause`：原因编码，最高位标识中断/异常；低位为具体类型编号。
- `stval`：关联值，如故障地址或指令字。
- `sstatus`：S 态状态位，包含 `SIE/SPIE/SPP` 等，用于中断使能与返回路径状态恢复。
- `sie`：S 态可屏蔽具体中断源（如 `MIP_STIP` 使能时钟中断）。

## 处理流程与上下文

### 进入 S trap 的硬件步骤（U→S 典型）

1. 保存 `pc → sepc`、记录 `scause/stval`。
2. `sstatus.SPIE ← SIE`，然后清 `SIE=0`（处理中断期间屏蔽中断）。
3. `SPP ← 0/1`（记录来自 U 还是 S）。
4. `pc ← stvec`，跳转至 S trap 入口。

### 统一入口与上下文组织

- 采用 Direct 模式：`stvec = &__alltraps`。
- 在 `__alltraps` 中：
  - 分配一块栈空间并按既定顺序保存 `x0..x31` 与 CSR 至 `trapframe`。
  - `a0 ← sp` 作为 C 函数 `trap(struct trapframe*)` 的唯一参数。
  - 调用 `trap` 完成分发与处理。
  - 返回后在 `__trapret` 中按相反顺序恢复上下文，执行 `sret` 回到来处。

### `trapframe` 与 ABI 约束

```c
struct pushregs {
    uintptr_t zero, ra, sp, gp, tp, t0, t1, t2, s0, s1,
              a0, a1, a2, a3, a4, a5, a6, a7,
              s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
              t3, t4, t5, t6;
};
struct trapframe {
    struct pushregs gpr;
    uintptr_t status, epc, badvaddr, cause; // sstatus, sepc, stval, scause
};
```

该布局必须与汇编保存/恢复逻辑严格一致，保证 `RESTORE_ALL` 后寄存器与返回地址、状态位、下一条执行位置均可正确恢复。

## 代码结构与职责划分

- `kern/driver/clock.c(h)`：定时器驱动，提供 `clock_init/clock_set_next_event/get_time` 封装，基于 OpenSBI `sbi_set_timer`。
- `kern/driver/intr.c(h)`：中断总开关封装 `intr_enable/intr_disable`（读写 `sstatus.SIE`）。
- `kern/init/init.c`：内核初始化流程中调用 `idt_init`、`clock_init`、`intr_enable`。
- `kern/trap/trapentry.S`：trap 汇编入口，`SAVE_ALL/RESTORE_ALL/__alltraps/__trapret`。
- `kern/trap/trap.c(h)`：`idt_init` 设置 `stvec`，`trap` 分发 `interrupt_handler/exception_handler`，并提供调试输出。

## 初始化与执行流

### 内核初始化（`kern_init`）

1. 控制台与内核信息输出。
2. `idt_init()`：
   - `sscratch ← 0`（约定：S 态 trap 时 sscratch 为 0；U 态 trap 前可用其存内核栈地址）。
   - `stvec ← &__alltraps`（4B 对齐，Direct 模式）。
3. `pmm_init()` 完成物理内存管理。
4. `clock_init()`：
   - `set_csr(sie, MIP_STIP)` 使能 S 定时器中断。
   - `clock_set_next_event()` 设置第一拍时钟事件。
   - 初始化 `ticks = 0`。
5. `intr_enable()`：`set_csr(sstatus, SSTATUS_SIE)`，全局打开 S 态中断。

### 一次时钟中断的完整路径

1. 定时器到期（OpenSBI 根据 `stime_value` 触发）→ trap 至 `__alltraps`。
2. `SAVE_ALL` 保存上下文；`a0=sp` 传递 `trapframe*`。
3. 进入 `trap()`，根据 `tf->cause` 最高位区分中断/异常，调用 `interrupt_handler()`。
4. 在时钟中断分支：
   - `clock_set_next_event()` 安排下一次；
   - `++ticks`，每 `100` 次打印一次 `100 ticks`；可按题设扩展到 `打印 10 次后关机`。
5. 返回 `__trapret`，`RESTORE_ALL` 恢复现场，`sret` 回原执行流。

## 关键实现片段与说明

### `idt_init` 设置 trap 入口

```c
void idt_init(void) {
    extern void __alltraps(void);
    write_csr(sscratch, 0);
    write_csr(stvec, &__alltraps);
}
```

### 中断开关

```c
void intr_enable(void)  { set_csr(sstatus, SSTATUS_SIE); }
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
```

### 时钟初始化与下一拍

```c
static uint64_t timebase = 100000; // QEMU: 10MHz → 10000ns/拍，100Hz

void clock_init(void) {
    set_csr(sie, MIP_STIP);
    clock_set_next_event();
    ticks = 0;
    cprintf("++ setup timer interrupts\n");
}

void clock_set_next_event(void) {
    sbi_set_timer(get_time() + timebase);
}
```

### trap 分发与时钟处理

```c
static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        interrupt_handler(tf);
    } else {
        exception_handler(tf);
    }
}

void interrupt_handler(struct trapframe *tf) {
    intptr_t cause = (tf->cause << 1) >> 1;
    switch (cause) {
        case IRQ_S_TIMER:
            clock_set_next_event();
            if (++ticks % 100 == 0) {
                cprintf("100 ticks\n");
            }
            break;
        default:
            print_trapframe(tf);
    }
}
```

### 异常处理（断点/非法指令，实验留空处的参考实现）

```c
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
        case CAUSE_ILLEGAL_INSTRUCTION:
            cprintf("Illegal instruction at 0x%016lx\n", tf->epc);
            tf->epc += 4; // 跳过故障指令，避免死循环
            break;
        case CAUSE_BREAKPOINT:
            cprintf("Breakpoint at 0x%016lx\n", tf->epc);
            tf->epc += 4; // ebreak 为 4 字节
            break;
        default:
            print_trapframe(tf);
    }
}
```

说明：是否“跳过”取决于语义需求——对 `ecall` 一般 `sepc+=4`；对调试型 `ebreak` 常见做法是单步越过。此处与课程要求一致即可。

## 原子性与中断屏蔽

为避免在修改关键内核数据结构（如物理页管理器）时被中断打断，提供本地中断屏蔽封装：

```c
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}
static inline void __intr_restore(bool flag) {
    if (flag) { intr_enable(); }
}
```

在 `alloc_pages/free_pages` 等路径上应用 `local_intr_save/restore`，确保关键区原子。

## 实验验证与运行输出

在 QEMU 上运行（OpenSBI 驱动 M 态服务）：

```text
+ setup timer interrupts
100 ticks
100 ticks
...
```

可见每秒打印一次 `100 ticks`（在 `timebase=100000`、10MHz time 下）。

扩展验证（按实验要求）：
- 在 `IRQ_S_TIMER` 分支累计打印次数 `num`，当 `num==10` 调用 `<sbi.h>` 的关机函数（如 `sbi_shutdown()`）验证关机路径。

## 设计权衡与工程细节

- **stvec Direct vs Vectored**：本实验采用 Direct 简化路径；Vectored 在 1.10+ 标准引入，可减少分发开销，但需构造表并对齐索引。
- **SIE 层级**：`sstatus.SIE` 为总体使能，`sie` 细分来源屏蔽；trap 期间硬件自动清 SIE，返回时由 SPIE 恢复。
- **sscratch 约定**：S/U 源区分；可用于用户态 trap 时切换到内核栈。
- **恢复顺序**：先 CSR 再 GPR，最后恢复 `sp` 并 `sret`；任一不匹配会导致返回错误或异常嵌套。
- **定时器周期**：频率过高会放大中断处理开销；一般选择 CPU 时钟的 1% 左右，实验中设置为 100Hz，便于观察。

## 常见问题与排错建议

- 看不到时钟中断：
  - 检查 `sie` 是否开启 `MIP_STIP`；
  - 是否调用 `clock_set_next_event()`；
  - `stvec` 是否正确、对齐；
  - `sstatus.SIE` 是否开启；
  - OpenSBI 版本/仿真时间基是否与假设一致。
- `sret` 报错或陷入重复：
  - `sepc` 未按需求前移（如 `ecall/ebreak`），导致再次触发；
  - `SPP`/`SPIE` 状态不一致；
  - `trapframe` 布局与汇编不匹配。
- 打印混乱或死锁：
  - 中断嵌套导致重入；
  - 在临界区忘记屏蔽中断。

## 11. 结论

本实验在最小内核上实现了 RISC-V S 态 trap 处理框架，通过定时器中断进行功能验证，梳理了从硬件委托到软件分发的完整链路，明确了上下文保存/恢复的 ABI 约束，并在工程层面提供了原子性保障。该框架为后续系统调用、调度与内存异常处理奠定基础。

---

# 扩展练习 Challenge1：描述与理解中断流程

## 1. 描述ucore中处理中断异常的流程（从异常的产生开始）

### 1.1 异常产生阶段

- **硬件检测**：硬件检测到异常/中断事件（时钟中断、外部中断、系统调用、缺页异常等）
- **自动保存**：RISC-V硬件自动保存关键状态到CSR寄存器：
  - `sepc`：异常发生时的PC值
  - `scause`：异常原因码
  - `stval`：异常相关值（如缺页地址）
  - `sstatus`：处理器状态（包括中断使能位）
- **跳转**：根据 `stvec` 寄存器跳转到中断处理入口 `__alltraps`

**对应代码：**

```c
    .globl __alltraps
    .align(2)
__alltraps: #     extern void __alltraps(void);
<------------------->
// kern/trap/trap.c - idt_init()
void idt_init(void) {
    extern void __alltraps(void);
    write_csr(sscratch, 0);  // 设置sscratch为0，表明当前位于内核态
    write_csr(stvec, &__alltraps); // 核心操作：将中断向量基地址设置为__alltraps
}
```

### 1.2 现场保存阶段（SAVE_ALL）

- **栈空间分配**：`addi sp, sp, -36 * REGBYTES` 为trapframe分配288字节空间
- **保存通用寄存器**：按固定偏移量保存x0-x31寄存器到栈中
- **保存CSR寄存器**：保存sstatus、sepc、stval、scause到栈中
- **设置sscratch**：将原始sp保存到sscratch，清零sscratch表示内核态

**对应代码：**

```assembly
// kern/trap/trapentry.S - SAVE_ALL宏
.macro SAVE_ALL
    csrw sscratch, sp        # 保存当前sp到sscratch
    addi sp, sp, -36 * REGBYTES  # 分配36个寄存器空间（288字节）
    
    # 保存通用寄存器x0-x31
    STORE x0, 0*REGBYTES(sp)   # x0 (zero)
    STORE x1, 1*REGBYTES(sp)   # x1 (ra)
    # 跳过 x2 (sp)，稍后单独处理
    STORE x3, 3*REGBYTES(sp)   # x3 (gp)
    # ... 继续保存 x4-x31
    
    # 获取并保存CSR寄存器
    csrrw s0, sscratch, x0     # 获取原始sp值，同时清零sscratch
    csrr s1, sstatus           # 获取状态寄存器
    csrr s2, sepc              # 获取异常PC
    csrr s3, stval             # 获取异常值
    csrr s4, scause            # 获取异常原因
    
    # 保存到栈中
    STORE s0, 2*REGBYTES(sp)   # sp 在位置2
    STORE s1, 32*REGBYTES(sp)  # sstatus 在位置32
    STORE s2, 33*REGBYTES(sp)  # sepc 在位置33
    STORE s3, 34*REGBYTES(sp)  # stval 在位置34
    STORE s4, 35*REGBYTES(sp)  # scause 在位置35
.endm
```

### 1.3 参数传递阶段

- **传递地址**：`move a0, sp` 将trapframe地址放入a0寄存器
- **跳转C函数**：`jal trap` 跳转到C语言编写的trap()函数

**对应代码：**

```assembly
// kern/trap/trapentry.S - __alltraps
.globl __alltraps
.align(2)
__alltraps:
    SAVE_ALL
    move  a0, sp    # 将trapframe地址传递给C函数
    jal trap        # 跳转到C语言编写的trap()函数
```

### 1.4 C语言处理阶段

- **中断分发**：`trap_dispatch()` 根据cause寄存器判断是中断还是异常
- **具体处理**：调用 `interrupt_handler()` 或 `exception_handler()` 进行具体处理
- **状态修改**：根据需要修改trapframe中的寄存器值

**对应代码：**

```c
// kern/trap/trap.c - trap()函数
void trap(struct trapframe *tf) {
    trap_dispatch(tf);
}

// 中断分发函数
static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) { // 如果cause最高位为1，表示中断
        interrupt_handler(tf);
    } else { // 如果cause最高位为0，表示异常
        exception_handler(tf);
    }
}
```

### 1.5 现场恢复阶段（RESTORE_ALL）

- **恢复CSR寄存器**：从栈中恢复sstatus和sepc到对应寄存器
- **恢复通用寄存器**：按固定偏移量恢复x1-x31寄存器
- **恢复栈指针**：最后恢复sp寄存器
- **返回执行**：`sret` 指令从异常返回，恢复PC和处理器状态

**对应代码：**

```assembly
// kern/trap/trapentry.S - RESTORE_ALL宏和__trapret
.macro RESTORE_ALL
    LOAD s1, 32*REGBYTES(sp)  # 恢复sstatus
    LOAD s2, 33*REGBYTES(sp)  # 恢复sepc
    
    csrw sstatus, s1          # 写回sstatus寄存器
    csrw sepc, s2             # 写回sepc寄存器
    
    # 恢复通用寄存器x1-x31
    LOAD x1, 1*REGBYTES(sp)   # 恢复ra
    LOAD x3, 3*REGBYTES(sp)   # 恢复gp
    # ... 继续恢复 x4-x31
    LOAD x31, 31*REGBYTES(sp) # 恢复x31
    
    # 最后恢复sp
    LOAD x2, 2*REGBYTES(sp)   # 恢复sp
.endm

.globl __trapret
__trapret:
    RESTORE_ALL
    sret  # 从异常返回
```

## 2. mov a0，sp的目的是什么？

### 2.1 参数传递机制

- **RISC-V ABI规范**：函数第一个参数通过a0寄存器传递
- **地址传递**：sp指向栈中保存的trapframe结构体
- **C函数接收**：trap(struct trapframe *tf) 函数通过tf参数接收trapframe地址

**对应代码：**

```assembly
// kern/trap/trapentry.S
move  a0, sp    # 将栈指针（指向trapframe）放入a0寄存器
jal trap        # 跳转到C函数trap()
```

```c
// kern/trap/trap.c
void trap(struct trapframe *tf) {
    // tf 指向栈中保存的trapframe结构体
    trap_dispatch(tf);
}
```


```
栈中的内存布局（sp指向的位置）：
┌─────────────────┐ ← sp (指向这里)
│   x0 (zero)     │ 0*8 = 0
├─────────────────┤
│   x1 (ra)       │ 1*8 = 8
├─────────────────┤
│   x2 (sp)       │ 2*8 = 16
├─────────────────┤
│   x3 (gp)       │ 3*8 = 24
├─────────────────┤
│   ...           │ ...
├─────────────────┤
│   x31           │ 31*8 = 248
├─────────────────┤
│   sstatus       │ 32*8 = 256
├─────────────────┤
│   sepc          │ 33*8 = 264
├─────────────────┤
│   stval         │ 34*8 = 272
├─────────────────┤
│   scause        │ 35*8 = 280
└─────────────────┘
```


### 2.2 数据访问能力

- **完整状态访问**：C代码通过tf指针访问所有保存的寄存器值
- **CSR寄存器访问**：可以访问cause、epc、stval、sstatus等关键信息
- **状态修改**：可以修改trapframe中的值，影响中断返回后的状态

**对应代码：**

```c
// kern/trap/trap.c - 中断处理函数
void interrupt_handler(struct trapframe *tf) {
    int irq = tf->cause & ((1UL << 63) - 1);  // 通过tf访问cause寄存器
        case IRQ_S_TIMER:
            clock_set_next_event();//发生这次时钟中断的时候，我们要设置下一次时钟中断
            if (++ticks % TICK_NUM == 0) {
                print_ticks();
            }
            break;
}

// 异常处理函数
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {  // 通过tf访问cause寄存器
        case xxx:
        //  具体处理逻辑
        break;
}
```

### 2.3 环境切换

- **汇编到C**：实现从汇编环境到C语言环境的无缝切换
- **统一处理**：所有中断/异常都使用相同的C语言处理逻辑
- **灵活处理**：C代码可以根据具体中断类型进行不同的处理

## 3. SAVE_ALL中寄存器保存在栈中的位置是什么确定的？

### 3.1 栈布局设计

- **固定偏移量**：每个寄存器使用固定的偏移量（REGBYTES = 8字节）
- **顺序存储**：按寄存器编号顺序存储，跳过x2（sp）单独处理
- **CSR寄存器**：放在通用寄存器之后（从32*REGBYTES开始）

**对应代码：**

```assembly
// kern/trap/trapentry.S - SAVE_ALL宏中的具体存储位置
.macro SAVE_ALL
    addi sp, sp, -36 * REGBYTES  # 分配36个寄存器空间
    
    # 通用寄存器存储位置
    STORE x0, 0*REGBYTES(sp)     # x0 在 0*8 位置
    STORE x1, 1*REGBYTES(sp)    # x1 在 1*8 位置
    # 跳过 x2 (sp)，稍后单独处理
    STORE x3, 3*REGBYTES(sp)    # x3 在 3*8 位置
    # ... 继续到 x31
    
    # CSR寄存器存储位置
    STORE s0, 2*REGBYTES(sp)    # sp 在 2*8 位置
    STORE s1, 32*REGBYTES(sp)   # sstatus 在 32*8 位置
    STORE s2, 33*REGBYTES(sp)  # sepc 在 33*8 位置
    STORE s3, 34*REGBYTES(sp)  # stval 在 34*8 位置
    STORE s4, 35*REGBYTES(sp)  # scause 在 35*8 位置
.endm
```


### 3.3 与trapframe结构体对应

- **结构体定义**：trapframe结构体按相同偏移量定义字段
- **内存布局匹配**：栈中的布局与trapframe结构体完全对应
- **访问一致性**：C代码通过tf指针可以正确访问所有寄存器值

**对应代码：**

```c
// kern/trap/trap.h - trapframe结构体定义
struct pushregs {
    uintptr_t zero;  // x0 -> 0*8
    uintptr_t ra;    // x1 -> 1*8
    uintptr_t sp;    // x2 -> 2*8
    uintptr_t gp;    // x3 -> 3*8
    // ... 其他寄存器
};

struct trapframe {
    struct pushregs gpr;     // 通用寄存器部分
    uintptr_t status;        // sstatus -> 32*8
    uintptr_t epc;          // sepc -> 33*8
    uintptr_t badvaddr;     // stval -> 34*8
    uintptr_t cause;        // scause -> 35*8
};
```

## 4. 对于任何中断，__alltraps 中都需要保存所有寄存器吗？请说明理由。

我的回答：是，需要保存所有寄存器。

### 4.2 理由说明

#### 4.2.1 统一性要求（所有中断/异常都使用同一个入口点__alltraps）

- **统一入口点**：所有中断/异常都使用同一个入口点__alltraps
- **无法预知类型**：无法预先知道具体是哪种中断，因此必须保存所有状态
- **一致性保证**：确保所有中断处理流程的一致性

**对应代码：**

```assembly
// kern/trap/trapentry.S - 所有中断都使用同一个入口点
.globl __alltraps
.align(2)
__alltraps:  # 所有中断/异常的统一入口点
    SAVE_ALL  # 必须保存所有寄存器
    move  a0, sp
    jal trap
```

# 扩展练习 Challenge2：理解上下文切换机制

## 1. csrw sscratch, sp；csrrw s0, sscratch, x0 实现了什么操作，目的是什么？

### 1.1 具体操作分析

**对应代码：**

```assembly
// kern/trap/trapentry.S - SAVE_ALL宏
.macro SAVE_ALL
    csrw sscratch, sp        # 将当前sp保存到sscratch寄存器
    # ... 其他保存操作
    csrrw s0, sscratch, x0   # 获取sscratch中的sp值到s0，同时将sscratch清零
    # ... 继续保存操作
.endm
```

### 1.2 操作步骤详解

#### 步骤1：`csrw sscratch, sp`

- **操作**：将当前栈指针sp的值写入sscratch寄存器
- **目的**：保存中断发生时的原始栈指针
- **时机**：在分配新的栈空间之前

#### 步骤2：`csrrw s0, sscratch, x0`

- **操作**：原子性地执行两个操作：
  1. 将sscratch寄存器的值读取到s0寄存器
  2. 将x0（零寄存器）的值写入sscratch寄存器
- **目的**：
  - 获取原始sp值到s0，用于后续保存
  - 清零sscratch，表示当前位于内核态

### 1.3 设计目的

#### 1.3.1 栈指针保护

- **原始sp保存**：确保能够恢复中断发生时的栈状态
- **内核栈切换**：为内核中断处理分配新的栈空间
- **状态标识**：通过sscratch的值区分用户态和内核态

#### 1.3.2 中断嵌套支持

- **嵌套检测**：sscratch为0表示当前在内核态，支持中断嵌套
- **状态切换**：从用户态（sscratch有值）切换到内核态（sscratch为0）
- **递归处理**：支持内核中断处理过程中的新中断

**对应代码：**

```assembly
// kern/trap/trapentry.S - 中断嵌套处理
.macro SAVE_ALL
    csrw sscratch, sp        # 保存用户态sp
    addi sp, sp, -36 * REGBYTES  # 分配内核栈空间
    # ... 保存寄存器
    csrrw s0, sscratch, x0   # 获取原始sp，清零sscratch
    STORE s0, 2*REGBYTES(sp) # 将原始sp保存到内核栈
.endm
```

### 1.4 上下文切换机制

#### 1.4.1 用户态到内核态切换

```
用户态：sscratch = 用户栈指针
内核态：sscratch = 0
```

#### 1.4.2 栈指针管理

- **用户栈**：保存用户态的执行上下文
- **内核栈**：保存内核态的中断处理上下文
- **切换机制**：通过sscratch寄存器实现栈指针的保存和恢复

## 2. save all里面保存了stval scause这些csr，而在restore all里面却不还原它们？那这样store的意义何在呢？

### 2.1 保存的CSR寄存器分析

**对应代码：**

```assembly
// kern/trap/trapentry.S - SAVE_ALL中保存CSR寄存器
.macro SAVE_ALL
    # ... 保存通用寄存器
    
    # 获取CSR寄存器值
    csrr s1, sstatus         # 获取状态寄存器
    csrr s2, sepc            # 获取异常PC
    csrr s3, stval           # 获取异常值
    csrr s4, scause          # 获取异常原因
    
    # 保存到栈中
    STORE s1, 32*REGBYTES(sp)  # sstatus
    STORE s2, 33*REGBYTES(sp)  # sepc
    STORE s3, 34*REGBYTES(sp)  # stval
    STORE s4, 35*REGBYTES(sp)  # scause
.endm
```

### 2.2 RESTORE_ALL中不恢复的原因

**对应代码：**

```assembly
// kern/trap/trapentry.S - RESTORE_ALL只恢复部分CSR
.macro RESTORE_ALL
    LOAD s1, 32*REGBYTES(sp)  # 恢复sstatus
    LOAD s2, 33*REGBYTES(sp)  # 恢复sepc
    
    csrw sstatus, s1          # 写回sstatus
    csrw sepc, s2             # 写回sepc
    
    # 注意：没有恢复stval和scause！
    # ... 恢复通用寄存器
.endm
```

### 2.3 不恢复stval和scause的原因

#### 2.3.1 硬件自动管理

- **stval（Supervisor Trap Value）**：硬件在异常发生时自动设置，异常返回后自动清零
- **scause（Supervisor Cause）**：硬件在异常发生时自动设置，异常返回后自动清零
- **硬件行为**：这些寄存器在异常返回时由硬件自动处理，不需要软件干预

#### 2.3.2 异常处理特性

- **一次性信息**：stval和scause包含的是异常发生时的瞬时信息
- **异常返回**：异常返回后，这些信息不再有意义
- **硬件清零**：sret指令执行后，硬件会自动清零这些寄存器

### 2.4 保存stval和scause的意义

#### 2.4.1 异常诊断和调试

- **异常信息**：提供异常发生时的详细信息
- **调试支持**：帮助开发者理解异常的原因和上下文
- **错误处理**：根据异常类型进行相应的处理

**对应代码：**

```c
// kern/trap/trap.c - 使用保存的CSR信息进行异常处理
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {  // 使用保存的scause
        case CAUSE_ILLEGAL_INSTRUCTION:
            // 使用tf->badvaddr（保存的stval）获取非法指令地址
            cprintf("Illegal instruction at 0x%lx\n", tf->badvaddr);
            break;
        case CAUSE_FAULT_LOAD:
            // 使用tf->badvaddr获取缺页地址
            cprintf("Page fault at 0x%lx\n", tf->badvaddr);
            break;
    }
}
```

#### 2.4.2 异常处理逻辑

- **异常分类**：根据scause判断异常类型
- **地址信息**：使用stval获取异常相关的地址信息
- **状态分析**：通过sstatus分析异常发生时的处理器状态

**对应代码：**

```c
// kern/trap/trap.c - 中断分发使用保存的CSR信息
static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {  // 使用保存的scause
        interrupt_handler(tf);
    } else {
        exception_handler(tf);
    }
}
```

#### 2.4.3 调试和诊断功能

- **完整上下文**：提供异常发生时的完整处理器状态
- **问题定位**：帮助定位异常发生的具体位置和原因
- **性能分析**：支持异常处理的性能分析

**对应代码：**

```c
// kern/trap/trap.c - 调试函数使用保存的CSR信息
void print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);    // 保存的sstatus
    cprintf("  epc      0x%08x\n", tf->epc);      // 保存的sepc
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr); // 保存的stval
    cprintf("  cause    0x%08x\n", tf->cause);    // 保存的scause
}
```

### 2.5 设计原理总结

#### 2.5.1 硬件软件分工

- **硬件负责**：stval和scause的自动设置和清零
- **软件负责**：保存这些信息用于异常处理和分析
- **分工明确**：硬件处理异常机制，软件处理异常逻辑

#### 2.5.2 异常处理流程

1. **异常发生**：硬件自动设置stval和scause
2. **现场保存**：软件保存这些CSR值到栈中
3. **异常处理**：软件使用保存的信息进行异常处理
4. **异常返回**：硬件自动清零stval和scause

#### 2.5.3 系统设计优势

- **信息完整**：保存完整的异常上下文信息
- **处理灵活**：软件可以根据异常信息进行灵活处理
- **调试友好**：提供丰富的调试和诊断信息
- **硬件优化**：利用硬件特性简化软件实现

## 总结

### csrw sscratch, sp；csrrw s0, sscratch, x0 的作用：

1. **栈指针保护**：保存用户态栈指针，支持内核栈切换
2. **状态标识**：通过sscratch区分用户态和内核态
3. **中断嵌套**：支持内核中断处理过程中的新中断
4. **上下文切换**：实现用户态到内核态的无缝切换

### 保存stval和scause但不恢复的原因：

1. **硬件管理**：这些寄存器由硬件自动设置和清零
2. **异常处理**：软件需要这些信息进行异常分类和处理
3. **调试支持**：提供完整的异常上下文信息
4. **设计分工**：硬件负责异常机制，软件负责异常逻辑

这种设计体现了操作系统内核中硬件和软件的分工协作，既保证了系统的稳定性，又提供了丰富的异常处理能力。

# 扩展练习Challenge3：完善异常中断

编程完善在触发一条非法指令异常 mret和，在 kern/trap/trap.c的异常处理函数中捕获，并对其进行处理，简单输出异常类型和异常指令触发地址，即“Illegal instruction caught at 0x(地址)”，“ebreak caught at 0x（地址）”与“Exception type:Illegal instruction"，“Exception type: breakpoint”。

## 代码补全

在 exception_handler 的 switch 语句中，为非法指令和断点异常添加了相应的处理逻辑。核心思路是利用 trapframe 提供的上下文信息（如 epc 寄存器保存的异常指令地址），打印诊断信息，并手动调整 epc 以跳过问题指令。具体实现如下所示：

```c#
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
        case CAUSE_MISALIGNED_FETCH:
            break;
        case CAUSE_FAULT_FETCH:
            break;
        case CAUSE_ILLEGAL_INSTRUCTION:
             // 非法指令异常处理
             /* LAB3 CHALLENGE3   2312145 :  */
            /*(1)输出指令异常类型（ Illegal instruction）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
            */
            cprintf("Exception type: Illegal instruction\n");
            cprintf("Illegal instruction caught at 0x%08x\n", tf->epc);
            tf->epc += 4;  // 更新epc指向下一条指令
            break;
        case CAUSE_BREAKPOINT:
            //断点异常处理
            /* LAB3 CHALLENGE3   2312145 :  */
            /*(1)输出指令异常类型（ breakpoint）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
            */
            cprintf("Exception type: breakpoint\n");
            cprintf("ebreak caught at 0x%08x\n", tf->epc);
            tf->epc += 2;  // ebreak指令长度为2字节(压缩指令)
            break;
……
```

对于非法指令异常（**CAUSE_ILLEGAL_INSTRUCTION**）：

1. 首先，使用 cprintf 输出异常类型："Exception type: Illegal instruction\n"，以明确标识异常性质。
2. 其次，打印异常指令的地址："Illegal instruction caught at 0x%08x\n"，其中 tf->epc 提供了精确的虚拟地址，帮助定位问题代码。
3. 最后，更新 tf->epc += 4; 这假设非法指令是标准 4 字节长度（RISC-V 的非压缩指令），通过将 epc 指向下一条指令，实现跳过并恢复执行。

对于断点异常（**CAUSE_BREAKPOINT**，通常由 ebreak 指令触发）：

1. 同样，先输出异常类型："Exception type: breakpoint\n"，表明这是一个调试断点。
2. 然后，打印地址："ebreak caught at 0x%08x\n"，使用 tf->epc 显示 ebreak 指令的位置。
3. 最后，更新 tf->epc += 2; 这里考虑到 ebreak 通常是 RISC-V C 扩展（压缩指令）的 2 字节形式，因此只前进 2 字节，以避免跳过过多代码导致执行错误。

这些代码确保了异常处理的原子性和可恢复性：在内核模式下，硬件已禁用中断（sstatus.SIE 清零），处理完成后，sret 指令会根据更新后的 epc 恢复程序流。同时，我注意到了指令长度的差异——非法指令可能为标准长度，而 ebreak 常为压缩形式，这基于 RISC-V 规范的观察，避免了潜在的 PC 对齐问题。

## 结果测试

在kernel_init函数中增加测试非法指令异常和测试断点异常的代码，在 clock_init() 和 intr_enable() 之后插入测试代码此时中断已启用，stvec已指向 __alltraps。同时，测试置于空闲循环之前，避免干扰后续内核运行。

```c
int kern_init(void) {
 ……
    clock_init();   // init clock interrupt
    intr_enable();  // enable irq interrupt

    // --------LAB3 CHALLENGE3: 测试异常处理 2312145------------------------------------
    cprintf("\n========== Testing Exception Handlers ==========\n");
    
    // 测试非法指令异常 (mret)
    cprintf("Testing illegal instruction (mret)...\n");
    asm volatile("mret");
    cprintf("Continued after illegal instruction\n\n");
    
    // 测试断点异常 (ebreak)
    cprintf("Testing breakpoint (ebreak)...\n");
    asm volatile("ebreak");
    cprintf("Continued after breakpoint\n");
    
    cprintf("========== Exception Tests Completed ==========\n\n");
    // --------------------------------------------------------------------------------
……
}
```

测试结果如下：

```bash
========== Testing Exception Handlers ==========
Testing illegal instruction (mret)...
sbi_emulate_csr_read: hartid0: invalid csr_num=0x302
Exception type: Illegal instruction
Illegal instruction caught at 0xc02000b4
Continued after illegal instruction

Testing breakpoint (ebreak)...
Exception type: breakpoint
ebreak caught at 0xc02000d0
Continued after breakpoint
========== Exception Tests Completed ==========
```

​	对于非法指令测试，mret 是 M 模式的返回指令。在S 模式下执行 mret 是非法的，会触发 Illegal Instruction 异常。这符合 RISC-V Privileged ISA 规范：特权指令在低权限模式下非法。程序能捕获后打印类型和地址（tf->epc），然后 tf->epc += 4; 跳过 4 字节（mret 是标准指令长度），允许程序继续执行到下一条 cprintf。这实现了“非致命”异常恢复，代码逻辑无误。

​	对于断点异常测试，ebreak 指令专用于触发 Breakpoint 异常。在 S 模式下执行会正确路由到 exception_handler。代码可以打印类型和地址，然后 tf->epc += 2; 跳过 2 字节。然后继续执行到下一条 cprintf，证明恢复成功。

​	综上所述，验证成功。
