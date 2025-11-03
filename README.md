# 2025-2026 操作系统课程实验# 2025-2026 操作系统课程 



## 👥 小组成员（按学号排序）本仓库当前对授课老师与学长学姐展示的仅相关于 **Lab1 作业提交** 的必要内容。请注意：



- **2312325 巩岱松**## 目录角色说明

- **2311561 梁朝阳**- `labcode/` 目录：授课老师与学长学姐提供的参考 / 原始教学代码（不作为本次提交成果主体）。

- **2312145 郭子涵**- `lab1/` 目录：本组的 Lab1 作业完整实现与报告；其中报告文件名称：`OS Lab1.md`（另附导出 PDF）。



---## 作者（按学号）

- 2312325 巩岱松

## 📚 仓库结构- 2311561 梁朝阳

- 2312145 郭子涵

```

.## 提交说明

├── lab1/                     # 实验一：系统启动与内核初始化- 请批改时以 `lab1/` 下代码与 `OS Lab1.md` 报告为准。

│   ├── OS Lab1.md           # 实验报告（Markdown）- `labcode/` 仅作对照参考，未删除其原始结构，未与我们修改产物混淆。

│   ├── OS Lab1.pdf          # 实验报告（PDF）- 本次提交中未上传与实验无关的本地环境脚本、临时构建产物或私有工具链目录，相关内容已在本地安全保留。

│   ├── kern/                # 内核代码（entry.S, init.c）

│   ├── libs/                # 基础库（printf, string, sbi）## 重点声明

│   └── tools/               # 构建工具（链接脚本等）本仓库的非 Lab1 其他实验（如后续 lab2 / lab3）暂未公开上传

│

├── lab2/                     # 实验二：物理内存管理## 仓库结构（当前公开部分）

│   ├── lab2实验报告.md       # 实验报告（Markdown）

│   ├── lab2实验报告.pdf      # 实验报告（PDF）```

│   ├── SLUB代码详解.md       # SLUB分配器详细文档.

│   ├── SLUB扩展练习总结.md   # 扩展练习总结├── lab1/                     # 本次提交的 Lab1 实现与报告

│   ├── kern/mm/             # 内存管理代码│   ├── Makefile              # 构建脚本

│   │   ├── pmm.c/h          # 物理内存管理器│   ├── OS Lab1.md            # 实验报告（源 Markdown）

│   │   ├── slub.c/h         # SLUB分配器实现│   ├── OS Lab1.pdf           # 报告导出（阅读版）

│   │   └── slub_test.c      # SLUB测试套件│   ├── kern/                 # 内核入口与基础代码 (entry.S, init.c 等)

│   └── assets/              # 报告配图│   ├── libs/                 # printf / string / sbi 等支撑库

││   ├── tools/                # 链接脚本、函数构建片段

├── lab3/                     # 实验三：中断与中断处理（当前重点）│   └── assets/               # 报告插图

│   ├── 实验三报告-中断与中断处理流程.md    # 实验报告（Markdown）├── labcode/                  # 助教提供的参考原始代码（未修改评测主体）

│   ├── OS第三次实验报告.md                  # 完整实验报告│   └── lab1/                 # 参考的 Lab1 原始版本

│   ├── 要求.md                              # 实验要求└── README.md                 # 当前说明文件

│   ├── kern/```

│   │   ├── trap/            # 中断处理核心

│   │   │   ├── trap.c/h     # 中断分发与处理（说明：其它未追踪或未展示目录为本地辅助环境/工具链，未纳入提交范围。）

│   │   │   └── trapentry.S  # 中断入口汇编代码

│   │   ├── driver/          # 硬件驱动## Lab1 简要概括

│   │   │   ├── clock.c/h    # 时钟驱动本次 Lab1 主要目标：

│   │   │   ├── console.c/h  # 控制台驱动1. 复现并理解 RISC-V 平台上从复位到内核入口的最小启动路径。

│   │   │   └── intr.c/h     # 中断控制器2. 解析 `entry.S` 中栈初始化与跳转 `kern_init()` 的指令级过程。

│   │   ├── debug/           # 调试支持3. 利用 OpenSBI + QEMU + GDB 逐地址验证控制权转移：0x1000 → 0x80000000 → 0x80200000。

│   │   │   ├── kdebug.c/h   # 内核调试4. 理解链接脚本如何组织 `.text / .data / .bss` 段并支撑 BSS 清零逻辑。

│   │   │   └── kmonitor.c/h # 内核监视器5. 对启动链关键问题（栈建立、内核入口、固件职责、特权级转换）给出可验证答案。

│   │   └── sync/            # 同步机制

│   │       └── sync.h       # 同步原语产出亮点：

│   └── tools/               # 构建工具- 报告中给出流程图、反汇编截取、内存布局示意、OpenSBI 输出解析。

│- 将辅助固件（OpenSBI）与最小内核行为拆分定位，强调教学内核的“可验证性”。

└── labcode/                  # 参考代码（教学原始代码）- 区分 `lab1/`（本次提交成果）与 `labcode/lab1/`（参考基线），避免混淆。

    ├── lab1/                # Lab1 参考实现

    ├── lab2/                # Lab2 参考实现如需更精简说明，可在评阅后告知，我们可以再提供 300 字以内摘要版本。

    └── lab3/                # Lab3 参考实现

```---

如有任何疑问，请联系作者之一；再次强调：**本次作业的核心成果在 `lab1/`，报告文件名为 `OS Lab1.md`。**

---谢谢老师和学长学姐！



## 🔬 实验概览

### Lab1: 系统启动与内核初始化
**目标**: 理解 RISC-V 平台从硬件复位到内核入口的完整启动流程

**关键内容**:
- RISC-V 启动流程分析（0x1000 → 0x80000000 → 0x80200000）
- OpenSBI 固件与内核的交互机制
- 栈初始化与内核入口点实现
- 链接脚本与内存布局设计

**成果**: 可验证的最小内核，完整启动流程文档

---

### Lab2: 物理内存管理
**目标**: 实现完整的物理内存管理系统

**关键内容**:
- First-Fit 和 Best-Fit 内存分配算法
- 页表机制与虚拟内存映射
- **扩展**: SLUB 分配器完整实现（~1600行代码）
  - 快速路径与慢速路径优化
  - CPU 本地缓存（92.4% 命中率）
  - 批量分配机制（性能提升 37.5%）

**成果**: 功能完备的内存管理器，详细的 SLUB 技术文档

---

### Lab3: 中断与中断处理 ⭐ **当前重点**

#### 📖 实验背景

中断处理机制是操作系统的核心功能之一。操作系统作为计算机系统的监管者，必须能对系统状态的突发变化做出反应——无论是程序执行异常，还是外设的突发请求。本实验在已有的物理内存管理基础上，加入对**中断与异常**的完整支持，并通过**时钟中断**验证中断处理系统的正确性。

#### 🎯 实验目标

1. **深入理解 RISC-V 中断机制**
   - 掌握 RISC-V 的中断分类（异常、陷入、外部中断）
   - 理解特权级模式（M/S/U 模式）及其切换机制
   - 学习中断委托机制（mideleg/medeleg）的工作原理

2. **实现上下文切换**
   - 理解中断帧（TrapFrame）的结构设计
   - 编写汇编代码实现上下文保存与恢复
   - 掌握关键寄存器（sepc, scause, stval, sstatus）的使用

3. **实现中断处理程序**
   - 编写时钟中断处理程序
   - 实现中断分发机制
   - 掌握从硬件触发到软件响应的完整流程

#### 🔑 核心技术要点

##### 1. 中断入口点实现
```c
void idt_init(void) {
    extern void __alltraps(void);
    write_csr(sscratch, 0);        // 初始化 sscratch
    write_csr(stvec, &__alltraps); // 设置中断向量表
}
```

##### 2. 上下文切换机制（trapentry.S）
```assembly
__alltraps:
    SAVE_ALL                    # 保存所有寄存器到栈
    move  a0, sp               # trapframe 指针作为参数
    jal trap                   # 调用 C 语言处理函数
    
__trapret:
    RESTORE_ALL                # 恢复所有寄存器
    sret                       # 从 S 模式返回
```

**关键设计**:
- 使用 `TrapFrame` 结构体保存 32 个通用寄存器 + 4 个 CSR 寄存器
- `sscratch` 暂存栈指针，实现零开销上下文切换
- 严格的保存/恢复顺序确保寄存器状态完整性

##### 3. 中断分发机制（trap.c）
```c
void trap(struct trapframe *tf) {
    trap_dispatch(tf);
}

static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        interrupt_handler(tf);  // 处理中断
    } else {
        exception_handler(tf);  // 处理异常
    }
}
```

**分发策略**:
- 通过 `scause` 寄存器的最高位区分中断/异常
- 根据 `cause` 值路由到具体处理函数
- 支持扩展的中断类型处理

##### 4. 时钟中断处理
```c
void interrupt_handler(struct trapframe *tf) {
    intptr_t cause = (tf->cause << 1) >> 1;
    switch (cause) {
        case IRQ_S_TIMER:
            clock_set_next_event();  // 设置下次中断
            ticks_count++;           // 计数器加一
            
            if (ticks_count == 100) {
                print_ticks();       // 输出 "100 ticks"
                ticks_count = 0;
                print_count++;
                
                if (print_count == 10) {
                    sbi_shutdown();  // 10 次后关机
                }
            }
            break;
    }
}
```

**时钟机制**:
- 通过 OpenSBI 的 `sbi_set_timer()` 设置时钟事件
- 每 10ms 触发一次中断（100 次/秒）
- 累计 100 次输出一次信息，共输出 10 次后系统关机

#### 📊 实验结果

**系统启动输出**:
```
OpenSBI v0.9
...
(THU.CST) os is loading ...
Special kernel symbols:
  entry  0x000000008020000a (virtual)
  etext  0x0000000080201a28 (virtual)
  edata  0x0000000080204010 (virtual)
  end    0x0000000080204028 (virtual)
Kernel executable memory footprint: 17KB
memory management: default_pmm_manager
physcial memory map:
  memory: 0x0000000007e00000, [0x0000000080204000, 0x0000000088004000).
check_alloc_page() succeeded!
check_pgdir() succeeded!
check_boot_pgdir() succeeded!
++ setup timer interrupts
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
[系统自动关机]
```

**验证要点**:
- ✅ 中断系统正常初始化
- ✅ 时钟中断稳定触发（每秒 1 次输出）
- ✅ 计数器准确工作
- ✅ 系统在第 10 次输出后正确关机

#### 🔧 技术难点与解决方案

| 难点 | 解决方案 |
|------|----------|
| **寄存器保存顺序** | 先保存 sp 到 sscratch，分配栈空间后保存其他寄存器 |
| **栈指针一致性** | 使用 `move a0, sp` 传递参数，确保函数调用前后 sp 不变 |
| **中断嵌套** | 进入 S 模式时自动清空 SIE，禁用中断嵌套 |
| **时钟精度** | 通过 `rdtime` 指令获取高精度时间戳 |

#### 👨‍💻 实验分工

- **巩岱松（2312325）**: 练习一 - 中断入口点与上下文切换实现
- **梁朝阳（2311561）**: Challenge 1 + Challenge 2 - 扩展中断处理机制
- **郭子涵（2312145）**: Challenge 3 - 高级调试与性能优化

#### 💡 技术收获

1. **中断机制理解深化**
   - 硬件层面：CPU 如何检测和响应中断，中断向量表的作用
   - 软件层面：规范的中断处理程序编写，上下文切换实现细节

2. **操作系统设计认识**
   - 模块化和分层设计原则
   - 硬件抽象层的重要性
   - 性能优化考虑（中断处理开销、系统响应性）

3. **RISC-V 架构掌握**
   - CSR 寄存器的使用方法
   - 特权级切换机制
   - OpenSBI 接口调用

#### 📝 相关文档

- **实验报告**: `lab3/实验三报告-中断与中断处理流程.md`
- **完整报告**: `lab3/OS第三次实验报告.md`
- **实验要求**: `lab3/要求.md`

---

## 🚀 如何运行

### 环境要求
- **操作系统**: Windows 10/11 + WSL2 或 Linux
- **工具链**: RISC-V GNU Toolchain
- **模拟器**: QEMU for RISC-V
- **调试工具**: GDB for RISC-V

### 编译与运行
```bash
# 进入实验目录
cd lab3

# 激活 RISC-V 环境（如果需要）
source /path/to/riscv_env.sh

# 编译
make

# 运行
make qemu

# 调试模式
make qemu-gdb
# 在另一个终端运行 GDB
riscv64-unknown-elf-gdb bin/kernel
```

---

## 📖 参考资料

- [RISC-V 特权级规范](https://riscv.org/technical/specifications/)
- [OpenSBI 文档](https://github.com/riscv-software-src/opensbi)
- [uCore 教学操作系统](https://github.com/chyyuu/os_kernel_lab)

---

## 📮 联系方式

如有任何疑问，请联系小组成员：
- 巩岱松：2312325@buaa.edu.cn
- 梁朝阳：2311561@buaa.edu.cn  
- 郭子涵：2312145@buaa.edu.cn

---

**最后更新**: 2025年11月3日  
**当前进度**: Lab3 实验完成 ✅
