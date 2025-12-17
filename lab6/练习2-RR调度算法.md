# Lab6 练习2：实现 Round Robin 调度算法 - 实验报告

**学号：2311561**

---

## 一、Lab5 与 Lab6 函数实现对比分析

### 1.1 对比函数：`schedule()` 函数的变化

#### Lab5 的 schedule() 实现（简化的FIFO）
```c
// Lab5 中，调度逻辑直接耦合在 schedule() 函数中
void schedule(void) {
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;
        // 直接遍历进程链表，找到下一个RUNNABLE进程
        do {
            if ((le = list_next(le)) != &proc_list) {
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE) {
                    break;
                }
            }
        } while (le != last);
        if (next == NULL || next->state != PROC_RUNNABLE) {
            next = idleproc;
        }
        next->runs++;
        if (next != current) {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}
```

#### Lab6 的 schedule() 实现（解耦框架）
```c
// Lab6 中，调度逻辑通过调度类接口实现
void schedule(void) {
    bool intr_flag;
    struct proc_struct *next;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        // 将当前进程重新加入就绪队列
        if (current->state == PROC_RUNNABLE) {
            sched_class_enqueue(current);  // 调用调度类的 enqueue
        }
        // 选择下一个进程
        if ((next = sched_class_pick_next()) != NULL) {  // 调用调度类的 pick_next
            sched_class_dequeue(next);  // 调用调度类的 dequeue
        }
        if (next == NULL) {
            next = idleproc;
        }
        next->runs++;
        if (next != current) {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}
```

### 1.2 为什么要做这个改动？

#### 改动的原因：

1. **职责分离（Separation of Concerns）**
   - **Lab5**：调度策略（如何选择进程）和调度机制（何时调度）混在一起
   - **Lab6**：调度框架只负责"何时调度"，具体"如何选择"交给调度类实现

2. **可扩展性（Extensibility）**
   - **Lab5**：如果要实现新的调度算法（如优先级调度、Stride调度），需要修改 `schedule()` 函数本身
   - **Lab6**：只需实现新的调度类，不需要修改框架代码

3. **代码复用（Code Reuse）**
   - **Lab6** 通过函数指针实现了类似面向对象的多态机制
   - 同一套框架可以支持多种调度算法

#### 不做这个改动会出现的问题：

1. **维护困难**
   ```c
   // 如果不解耦，schedule() 会变成这样：
   void schedule(void) {
       if (使用RR算法) {
           // RR 的选择逻辑
       } else if (使用Stride算法) {
           // Stride 的选择逻辑
       } else if (使用CFS算法) {
           // CFS 的选择逻辑
       }
       // ... 代码会变得臃肿且难以维护
   }
   ```

2. **违反开闭原则**
   - 每次添加新算法都要修改核心调度代码
   - 增加了引入 bug 的风险

3. **测试困难**
   - 不同算法的代码耦合在一起，难以单独测试
   - 修改一个算法可能影响其他算法

### 1.3 对比函数：`wakeup_proc()` 函数的变化

#### Lab5 实现
```c
void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
        }
        else {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}
```

#### Lab6 实现
```c
void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
            if (proc != current) {
                sched_class_enqueue(proc);  // 新增：加入就绪队列
            }
        }
        else {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}
```

**改动原因**：
- Lab5 中只是修改状态，由 `schedule()` 遍历进程链表时发现
- Lab6 中需要显式加入就绪队列，使调度器能快速找到可运行进程
- 提高了调度效率，避免了遍历整个进程链表

---

## 二、RR 调度算法实现详解

### 2.1 RR_init() - 初始化就绪队列

#### 实现思路
初始化运行队列的数据结构，为后续的调度操作做准备。

#### 代码实现
```c
static void
RR_init(struct run_queue *rq)
{
    // LAB6: 2311561
    list_init(&(rq->run_list));      // 初始化双向链表
    rq->lab6_run_pool = NULL;        // 斜堆指针置空（Stride用）
    rq->proc_num = 0;                // 进程数量初始化为0
}
```

#### 关键说明
1. **`list_init(&(rq->run_list))`**
   - 初始化双向循环链表的头节点
   - 使得 `run_list.prev` 和 `run_list.next` 都指向自己
   - 这是一个**哨兵节点**，简化边界条件处理

2. **为什么需要初始化斜堆指针？**
   - `lab6_run_pool` 用于 Stride 调度算法（练习3）
   - 虽然 RR 不使用，但需要初始化避免野指针

3. **边界条件处理**
   - 空队列状态：`run_list` 的 `next` 和 `prev` 指向自己
   - 此时 `list_empty()` 返回 true

---

### 2.2 RR_enqueue() - 进程入队

#### 实现思路
将进程插入到就绪队列的**末尾**，实现 FIFO 策略。如果进程的时间片已用完，重新分配时间片。

#### 代码实现
```c
static void
RR_enqueue(struct run_queue *rq, struct proc_struct *proc)
{
    // LAB6: 2311561
    assert(list_empty(&(proc->run_link)));  // 确保进程不在队列中
    assert(proc->rq == NULL);               // 确保进程未关联其他队列

    // 重新分配时间片
    if (proc->time_slice <= 0 || proc->time_slice > rq->max_time_slice)
    {
        proc->time_slice = rq->max_time_slice;
    }
    
    proc->rq = rq;                          // 关联到当前队列
    rq->proc_num++;                         // 队列进程数+1
    list_add_before(&(rq->run_list), &(proc->run_link));  // 插入队尾
}
```

#### 关键说明

1. **为什么使用 `list_add_before(&(rq->run_list), ...)`？**
   
   双向循环链表结构：
   ```
   run_list (哨兵)
       ↓
   [run_list] ←→ [P1] ←→ [P2] ←→ [P3] ←→ [run_list]
       ↑__________________________________________|
   ```
   
   - `list_add_before(&head, &new)` 将 `new` 插入到 `head` **之前**
   - 由于是循环链表，插入到头节点之前 = 插入到队尾
   - **队首**：`list_next(&run_list)` → P1
   - **队尾**：`list_prev(&run_list)` → P3

2. **时间片重置逻辑**
   ```c
   if (proc->time_slice <= 0 || proc->time_slice > rq->max_time_slice)
   ```
   - `time_slice <= 0`：时间片用完，重新分配
   - `time_slice > max_time_slice`：防止异常值，确保时间片在合理范围

3. **边界条件处理**
   - **空队列插入**：第一个进程成为队列中唯一元素
   - **断言保护**：确保进程不会重复入队（避免链表环）

4. **为什么选择队尾插入？**
   - RR 算法是 **FIFO + 时间片**
   - 时间片用完的进程应该排到队尾，保证公平性
   - 新就绪的进程也从队尾加入

---

### 2.3 RR_dequeue() - 进程出队

#### 实现思路
从就绪队列中移除指定进程，清理其队列关联信息。

#### 代码实现
```c
static void
RR_dequeue(struct run_queue *rq, struct proc_struct *proc)
{
    // LAB6: 2311561
    assert(proc->rq == rq);                    // 确保进程在此队列中
    assert(!list_empty(&(proc->run_link)));    // 确保进程在链表中

    list_del_init(&(proc->run_link));          // 从链表删除并重新初始化
    proc->rq = NULL;                           // 解除队列关联
    rq->proc_num--;                            // 队列进程数-1
}
```

#### 关键说明

1. **`list_del_init()` vs `list_del()`**
   ```c
   // list_del_init() 的作用：
   static inline void list_del_init(list_entry_t *listelm) {
       list_del(listelm);        // 从链表移除
       list_init(listelm);       // 重新初始化为空链表
   }
   ```
   
   - **为什么要 `init`？**
     - 移除后，节点的 `prev` 和 `next` 指针悬空
     - 重新初始化后，`list_empty()` 可以正确判断节点未在链表中
     - 下次 `enqueue` 时的断言 `assert(list_empty(&(proc->run_link)))` 会通过

2. **边界条件处理**
   - **只有一个进程时**：移除后队列变空，`run_list` 的 `next` 和 `prev` 重新指向自己
   - **断言保护**：确保不会误删除不在队列中的进程

3. **为什么需要清理 `proc->rq`？**
   - 表示进程已不在任何就绪队列中
   - 防止进程被重复操作
   - 便于调试（可以判断进程当前是否在某个队列）

---

### 2.4 RR_pick_next() - 选择下一个进程

#### 实现思路
从就绪队列的**队首**取出进程，实现 FIFO 选择策略。

#### 代码实现
```c
static struct proc_struct *
RR_pick_next(struct run_queue *rq)
{
    // LAB6: 2311561
    if (list_empty(&(rq->run_list)))       // 队列为空，返回NULL
    {
        return NULL;
    }
    list_entry_t *le = list_next(&(rq->run_list));  // 获取队首节点
    return le2proc(le, run_link);                    // 转换为进程控制块指针
}
```

#### 关键说明

1. **`le2proc` 宏的工作原理**
   ```c
   // 定义在 proc.h 中
   #define le2proc(le, member) \
       to_struct((le), struct proc_struct, member)
   
   // to_struct 的实现（在 defs.h 中）
   #define to_struct(ptr, type, member) \
       ((type *)((char *)(ptr) - offsetof(type, member)))
   ```
   
   **原理**：
   - `le` 是 `proc->run_link` 的地址
   - 通过 `offsetof` 计算 `run_link` 在 `proc_struct` 中的偏移量
   - 用 `le` 减去偏移量，得到 `proc_struct` 的起始地址

   **示意图**：
   ```
   proc_struct 的内存布局：
   +-------------------+  ← proc 地址
   | state             |
   | pid               |
   | ...               |
   | run_link          |  ← le 指向这里
   |   ├─ prev         |
   |   └─ next         |
   | ...               |
   +-------------------+
   
   le2proc(le, run_link) = le - offsetof(proc_struct, run_link)
                         = proc 地址
   ```

2. **为什么选择队首？**
   - RR 是 FIFO 策略：先进队的进程先被调度
   - 配合 `enqueue` 的队尾插入，实现轮转效果

3. **边界条件处理**
   - **空队列**：直接返回 `NULL`，调度器会选择 `idleproc`
   - **单进程**：返回唯一的进程，该进程会继续运行

---

### 2.5 RR_proc_tick() - 时间片处理

#### 实现思路
每次时钟中断时被调用，递减当前进程的时间片。时间片用完时设置 `need_resched` 标志，触发进程调度。

#### 代码实现
```c
static void
RR_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
    // LAB6: 2311561
    if (proc->time_slice > 0)           // 如果还有剩余时间片
    {
        proc->time_slice--;             // 时间片减1
    }
    if (proc->time_slice == 0)          // 时间片用完
    {
        proc->need_resched = 1;         // 设置调度标志
    }
}
```

#### 关键说明

1. **时钟中断的调用路径**
   ```
   时钟中断 (IRQ_S_TIMER)
       ↓
   interrupt_handler()
       ↓
   sched_class_proc_tick(current)
       ↓
   sched_class->proc_tick(rq, current)
       ↓
   RR_proc_tick(rq, current)  ← 这里
   ```

2. **为什么要先判断 `time_slice > 0`？**
   - 防止时间片减为负数
   - 时间片为 0 时不再递减，等待调度器处理
   - 保持状态的一致性

3. **`need_resched` 标志的作用**
   ```c
   // 在 trap() 函数中检查（trap.c:282）
   if (current->need_resched) {
       schedule();  // 触发调度
   }
   ```
   
   - **延迟调度**：不在中断处理函数中直接调度
   - **原子性保证**：中断处理完成后再调度
   - **避免嵌套**：防止在中断中再次触发中断

4. **为什么不在这里直接调用 `schedule()`？**
   ```
   错误做法：
   RR_proc_tick() {
       if (proc->time_slice == 0) {
           schedule();  // ❌ 不应该在这里调度！
       }
   }
   
   问题：
   1. 中断处理函数应该尽快返回
   2. schedule() 可能导致复杂的上下文切换
   3. 违反了"设置标志、延迟处理"的设计原则
   ```

5. **边界条件处理**
   - **idleproc 的处理**：在 `sched_class_proc_tick` 中特殊处理
     ```c
     void sched_class_proc_tick(struct proc_struct *proc) {
         if (proc != idleproc) {
             sched_class->proc_tick(rq, proc);
         }
         else {
             proc->need_resched = 1;  // idle进程总是可调度
         }
     }
     ```
   
   - **时间片为0的进程被再次调度时**：
     - 在 `RR_enqueue` 中会重新分配时间片
     - 保证每次运行都有完整的时间片

---

## 三、实验结果展示

### 3.1 make grade 输出

```
mac@macdeMacBook-Air-2 lab6 % make clean && make grade GDB=gdb-multiarch
rm -f -r obj bin
priority:                (3.6s)
  -check result:                             OK
  -check output:                             OK
Total Score: 50/50
```

**结果分析**：
- ✅ 所有测试通过
- ✅ 调度逻辑正确
- ✅ 输出符合预期

### 3.2 QEMU 运行日志分析

```
sched class: RR_scheduler
++ setup timer interrupts
kernel_execve: pid = 2, name = "priority".
set priority to 6
main: fork ok,now need to wait pids.
set priority to 1
set priority to 2
set priority to 3
set priority to 4
set priority to 5
child pid 3, acc 1188000, time 2010
child pid 4, acc 1172000, time 2010
child pid 5, acc 1192000, time 2010
child pid 6, acc 1176000, time 2020
child pid 7, acc 1152000, time 2020
main: pid 0, acc 1188000, time 2030
main: pid 4, acc 1172000, time 2030
main: pid 5, acc 1192000, time 2030
main: pid 6, acc 1176000, time 2030
main: pid 7, acc 1152000, time 2030
main: wait pids over
sched result: 1 1 1 1 1
```

#### 观察到的调度现象

1. **时间片轮转效果**
   - 各进程的执行时间相近（约2010-2030ms）
   - 说明每个进程获得了公平的 CPU 时间

2. **FIFO 顺序**
   - 进程按创建顺序被调度（PID 3→4→5→6→7）
   - 符合 RR 的队列特性

3. **调度结果均等**
   - `sched result: 1 1 1 1 1` 表示每个进程获得的调度次数相近
   - 验证了 RR 算法的公平性

4. **进程同步**
   - 父进程等待所有子进程完成
   - `do_wait()` 正确回收子进程

---

## 四、Round Robin 调度算法分析

### 4.1 优点

#### 1. 公平性（Fairness）
```
假设有3个进程 P1, P2, P3，时间片 = 5ms

时间轴：
0     5    10    15    20    25    30    35    40
|--P1--|--P2--|--P3--|--P1--|--P2--|--P3--|--P1--|--P2--|

每个进程都能公平获得 CPU 时间
```

**优势**：
- 每个进程都有均等的 CPU 使用机会
- 没有进程会饿死（starvation）
- 适合分时系统，用户体验良好

#### 2. 响应时间可预测
- 最长等待时间 = (n-1) × 时间片大小（n 为进程数）
- 例如：5个进程，时间片5ms → 最长等待 20ms

#### 3. 实现简单
- 只需一个 FIFO 队列
- 算法逻辑清晰，易于理解和维护
- 不需要复杂的优先级计算

#### 4. 交互友好
- 短时间内所有进程都能得到响应
- 适合交互式系统（终端、GUI）

### 4.2 缺点

#### 1. 不区分进程优先级
```
场景：
- 重要进程（视频播放）
- 普通进程（后台下载）

RR 给予相同的时间片 → 视频可能卡顿
```

**问题**：
- 无法满足实时性要求
- 重要任务得不到优先处理

#### 2. 平均周转时间可能较长
```
示例（时间片=1）：
进程   到达时间   CPU需求   完成时间   周转时间
P1     0         1         1          1
P2     0         100       200        200
P3     0         1         199        199
平均周转时间 = (1+200+199)/3 = 133.3

如果使用 SJF（最短作业优先）：
P1 → P3 → P2
平均周转时间 = (1+2+102)/3 = 35
```

#### 3. 上下文切换开销
```
假设：
- 进程切换时间 = 0.1ms
- 时间片 = 1ms
- CPU利用率 = 1 / (1 + 0.1) ≈ 91%

如果时间片 = 10ms：
- CPU利用率 = 10 / (10 + 0.1) ≈ 99%
```

**问题**：
- 时间片太小 → 频繁切换，开销大
- 需要权衡响应性和效率

#### 4. 对 I/O 密集型进程不友好
```
CPU密集型进程：用完整个时间片
I/O密集型进程：很快进入等待，时间片浪费

结果：I/O 进程等待时间长，响应慢
```

### 4.3 时间片大小的优化

#### 时间片过大的问题
```
时间片 = 100ms，3个进程
P1: 执行 5ms → 等待 I/O
P2: 执行 100ms（用完时间片）
P3: 执行 100ms（用完时间片）

P1 的响应时间 = 100 + 100 + 5 = 205ms  ← 太长！
退化为 FIFO
```

#### 时间片过小的问题
```
时间片 = 1ms，上下文切换 = 0.1ms
每秒可切换 1000 次进程
但实际 CPU 利用率只有 91%

大量时间浪费在切换上
```

#### 最佳实践

1. **经验法则**
   - 时间片应该是上下文切换时间的 **100-1000 倍**
   - 典型值：10-100ms

2. **动态调整**
   ```c
   // 可以根据系统负载动态调整
   if (proc_num < 5) {
       time_slice = 50ms;  // 进程少，时间片长
   } else {
       time_slice = 10ms;  // 进程多，时间片短，保证响应
   }
   ```

3. **考虑因素**
   - 系统类型（交互式 vs 批处理）
   - 进程数量
   - I/O 特性

#### uCore 中的时间片设置
```c
// sched.h
#define MAX_TIME_SLICE 5

// 每个时钟中断 = 10ms（大约）
// 实际时间片 ≈ 5 × 10ms = 50ms
```

**分析**：
- 50ms 的时间片适中
- 保证了响应性（最多等待 n×50ms）
- 减少了上下文切换开销

### 4.4 为什么需要在 RR_proc_tick 中设置 need_resched？

#### 原因1：延迟调度机制

```c
// 不好的设计（直接调度）
void RR_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    if (--proc->time_slice == 0) {
        schedule();  // ❌ 在中断处理中调度
    }
}

问题：
1. schedule() 可能很耗时
2. 中断处理应该尽快返回
3. 可能导致中断嵌套问题
```

```c
// 好的设计（设置标志）
void RR_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    if (--proc->time_slice == 0) {
        proc->need_resched = 1;  // ✅ 只设置标志
    }
}

// 在安全的时机检查
void trap(struct trapframe *tf) {
    // ... 中断处理 ...
    if (!in_kernel && current->need_resched) {
        schedule();  // 在返回用户态前调度
    }
}
```

#### 原因2：保证原子性

```
中断处理流程：
1. 保存现场（硬件自动）
2. 处理中断（proc_tick设置标志）
3. 恢复现场
4. 检查need_resched
5. 如果需要，调用schedule()

保证了中断处理的完整性
```

#### 原因3：统一调度入口

```c
// 多个地方可以设置 need_resched
1. RR_proc_tick()     - 时间片用完
2. do_yield()         - 主动让出
3. wakeup_proc()      - 唤醒高优先级进程

// 统一在 trap() 中检查
if (current->need_resched) {
    schedule();  // 统一入口，便于管理
}
```

#### 原因4：避免死锁

```
如果在中断中调度：
1. 中断禁用
2. 调用 schedule()
3. schedule() 中可能需要访问某些资源
4. 资源被锁住 → 死锁

使用标志延迟调度：
1. 中断处理完成
2. 返回前检查标志
3. 此时中断已启用，安全调度
```

---

## 五、拓展思考

### 5.1 实现优先级 RR 调度

#### 设计思路

**多级队列 + RR**：不同优先级使用不同队列，每个队列内部使用 RR。

#### 数据结构修改

```c
// 修改运行队列
#define MAX_PRIORITY 8

struct run_queue {
    list_entry_t run_list[MAX_PRIORITY];  // 多个队列
    unsigned int proc_num[MAX_PRIORITY];
    int max_time_slice[MAX_PRIORITY];     // 不同优先级不同时间片
};

// 修改进程结构
struct proc_struct {
    // ... 原有字段 ...
    int priority;  // 进程优先级 (0-7)
};
```

#### 修改调度类函数

```c
static void
PriorityRR_enqueue(struct run_queue *rq, struct proc_struct *proc)
{
    int prio = proc->priority;
    assert(prio >= 0 && prio < MAX_PRIORITY);
    
    // 加入对应优先级的队列
    list_add_before(&(rq->run_list[prio]), &(proc->run_link));
    
    // 设置时间片（高优先级时间片可以更长）
    if (proc->time_slice <= 0) {
        proc->time_slice = rq->max_time_slice[prio];
    }
    
    proc->rq = rq;
    rq->proc_num[prio]++;
}

static struct proc_struct *
PriorityRR_pick_next(struct run_queue *rq)
{
    // 从高优先级到低优先级查找
    for (int i = MAX_PRIORITY - 1; i >= 0; i--) {
        if (!list_empty(&(rq->run_list[i]))) {
            list_entry_t *le = list_next(&(rq->run_list[i]));
            return le2proc(le, run_link);
        }
    }
    return NULL;
}

static void
PriorityRR_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
    if (proc->time_slice > 0) {
        proc->time_slice--;
    }
    if (proc->time_slice == 0) {
        // 可选：动态调整优先级（防止饥饿）
        if (proc->priority > 0) {
            proc->priority--;  // 降低优先级
        }
        proc->need_resched = 1;
    }
}
```

#### 防止低优先级进程饥饿

```c
// 方案1：优先级提升
// 如果进程等待时间过长，提升其优先级
void aging_mechanism(struct proc_struct *proc) {
    proc->wait_time++;
    if (proc->wait_time > AGING_THRESHOLD) {
        if (proc->priority < MAX_PRIORITY - 1) {
            proc->priority++;
        }
        proc->wait_time = 0;
    }
}

// 方案2：多级反馈队列
// CPU密集型进程逐渐降级，I/O密集型进程保持高优先级
```

### 5.2 多核调度支持分析

#### 当前实现的局限性

```c
// 当前实现是单核的
static struct run_queue __rq;  // 全局只有一个运行队列

void schedule(void) {
    // 只考虑单个CPU的调度
    next = sched_class->pick_next(rq);
    proc_run(next);
}
```

**问题**：
1. **单一运行队列**：所有 CPU 共享一个队列，争用锁
2. **无 CPU 亲和性**：进程可能在不同 CPU 间频繁迁移
3. **负载不均衡**：没有负载均衡机制

#### 多核调度改进方案

##### 方案1：Per-CPU 运行队列

```c
// 每个 CPU 有自己的运行队列
struct run_queue per_cpu_rq[MAX_CPU_NUM];

void schedule(void) {
    int cpu_id = smp_processor_id();  // 获取当前CPU ID
    struct run_queue *rq = &per_cpu_rq[cpu_id];
    
    next = sched_class->pick_next(rq);
    if (next == NULL) {
        // 尝试从其他CPU窃取任务
        next = steal_process_from_other_cpu();
    }
    proc_run(next);
}
```

**优势**：
- 减少锁竞争
- 更好的缓存局部性
- CPU 亲和性

##### 方案2：负载均衡

```c
// 周期性负载均衡
void load_balance(void) {
    int cpu_id = smp_processor_id();
    struct run_queue *this_rq = &per_cpu_rq[cpu_id];
    
    // 找到最繁忙的CPU
    int busiest_cpu = find_busiest_cpu();
    if (busiest_cpu == cpu_id) return;
    
    struct run_queue *busiest_rq = &per_cpu_rq[busiest_cpu];
    
    // 迁移一些进程到当前CPU
    if (busiest_rq->proc_num > this_rq->proc_num + 1) {
        migrate_process(busiest_rq, this_rq);
    }
}
```

##### 方案3：进程亲和性

```c
struct proc_struct {
    // ... 原有字段 ...
    int cpu_affinity;      // CPU亲和性（位掩码）
    int last_cpu;          // 上次运行的CPU
};

// 调度时考虑亲和性
struct proc_struct *pick_next_with_affinity(struct run_queue *rq) {
    int cpu_id = smp_processor_id();
    list_entry_t *le = list_next(&(rq->run_list));
    
    while (le != &(rq->run_list)) {
        struct proc_struct *p = le2proc(le, run_link);
        // 检查是否允许在当前CPU运行
        if (p->cpu_affinity & (1 << cpu_id)) {
            return p;
        }
        le = list_next(le);
    }
    return NULL;
}
```

#### 需要修改的主要部分

1. **数据结构**
   ```c
   // sched.h
   struct run_queue per_cpu_rq[MAX_CPU_NUM];  // Per-CPU队列
   
   // proc.h
   struct proc_struct {
       int cpu_affinity;  // CPU亲和性
       int last_cpu;      // 上次运行的CPU
   };
   ```

2. **调度器框架**
   ```c
   // sched.c
   void sched_init(void) {
       for (int i = 0; i < MAX_CPU_NUM; i++) {
           sched_class->init(&per_cpu_rq[i]);
       }
   }
   
   void schedule(void) {
       int cpu_id = smp_processor_id();
       struct run_queue *rq = &per_cpu_rq[cpu_id];
       // ... 使用当前CPU的队列 ...
   }
   ```

3. **同步机制**
   ```c
   // 需要添加自旋锁保护队列
   struct run_queue {
       list_entry_t run_list;
       spinlock_t lock;  // 保护队列的锁
       // ...
   };
   
   void RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
       spin_lock(&rq->lock);
       // ... 入队操作 ...
       spin_unlock(&rq->lock);
   }
   ```

4. **负载均衡**
   ```c
   // 在时钟中断中周期性调用
   void interrupt_handler(struct trapframe *tf) {
       case IRQ_S_TIMER:
           // ...
           if (ticks % LOAD_BALANCE_INTERVAL == 0) {
               load_balance();
           }
           break;
   }
   ```

---

## 六、总结

### 6.1 实验收获

1. **深入理解了调度器框架设计**
   - 策略与机制分离
   - 面向接口编程
   - 函数指针实现多态

2. **掌握了 RR 调度算法的实现**
   - FIFO 队列管理
   - 时间片机制
   - 公平性保证

3. **理解了延迟调度机制**
   - `need_resched` 标志的作用
   - 中断处理与调度的配合
   - 原子性保证

4. **学习了数据结构的应用**
   - 双向循环链表
   - `le2proc` 宏技巧
   - 边界条件处理

### 6.2 代码质量要点

1. **完善的错误检查**
   ```c
   assert(list_empty(&(proc->run_link)));  // 防止重复入队
   assert(proc->rq == NULL);               // 防止多队列冲突
   ```

2. **良好的边界处理**
   - 空队列检查
   - 时间片边界值处理
   - idleproc 特殊处理

3. **清晰的代码注释**
   - 每个函数的功能说明
   - 关键步骤的解释
   - 学号标注

### 6.3 性能分析

**时间复杂度**：
- `enqueue`: O(1)
- `dequeue`: O(1)
- `pick_next`: O(1)
- `proc_tick`: O(1)

**空间复杂度**：O(n)（n 为进程数）

**优化空间**：
- 多核支持
- 优先级机制
- 动态时间片调整

---

## 七、参考资料

1. uCore 实验指导书
2. 操作系统概念（Operating System Concepts）
3. Linux 内核设计与实现
4. RISC-V 中断处理机制
5. 《深入理解计算机系统》

---

**实验完成时间**：2024年12月
**实验环境**：Ubuntu 22.04 / QEMU / RISC-V
**代码提交**：已通过 `make grade` 测试（50/50分）

