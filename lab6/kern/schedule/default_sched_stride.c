#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>
#include <stdio.h>
#include <skew_heap.h>

#define USE_SKEW_HEAP 1

/* 你应该在这里定义BigStride常量 */
/* LAB6 CHALLENGE 1: 2312145 */
#define BIG_STRIDE 0x7FFFFFFF /* 使用最大正整数，利用溢出特性 */

/* 用于比较两个skew_heap_node_t及其对应进程的比较函数 */
static int
proc_stride_comp_f(void *a, void *b)
{
     struct proc_struct *p = le2proc(a, lab6_run_pool);
     struct proc_struct *q = le2proc(b, lab6_run_pool);
     /* * 关键修正：必须强制转换为有符号整数 (int32_t) 
      * 否则两个很大的无符号数相减可能得到错误的结果，导致调度死循环（Timeout原因）
      */
     int32_t c = (int32_t)(p->lab6_stride - q->lab6_stride);
     if (c > 0)
          return 1;
     else if (c == 0)
          return 0;
     else
          return -1;
}

/*
 * stride_init 初始化运行队列rq，正确分配成员变量，包括：
 *
 * - run_list: 初始化后应为空列表。
 * - lab6_run_pool: NULL
 * - proc_num: 0
 * - max_time_slice: 无需在此处设置，由调用者分配。
 *
 * 提示: 参见 libs/list.h 中的列表结构例程。
 */
static void
stride_init(struct run_queue *rq)
{
     /* LAB6 CHALLENGE 1: 2312145
      * (1) 初始化就绪进程列表: rq->run_list
      * (2) 初始化运行池: rq->lab6_run_pool
      * (3) 设置进程数量: rq->proc_num 为 0
      */
     list_init(&(rq->run_list));
     rq->lab6_run_pool = NULL;
     rq->proc_num = 0;
}

/*
 * stride_enqueue 将进程 ``proc'' 插入运行队列 ``rq''。该过程应验证/初始化 ``proc'' 的相关成员，
 * 然后将 ``lab6_run_pool'' 节点放入队列（因为这里使用优先队列）。该过程还应更新 ``rq'' 结构的元数据。
 *
 * proc->time_slice 表示为进程分配的时间片，应设置为 rq->max_time_slice。
 *
 * 提示: 参见 libs/skew_heap.h 中的优先队列结构例程。
 */
static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc)
{
     /* LAB6 CHALLENGE 1: 2312145
      * (1) 正确地将proc插入rq
      * 注意: 你可以使用skew_heap或list。重要函数
      * skew_heap_insert: 将条目插入skew_heap
      * list_add_before: 将条目插入列表末尾
      * (2) 重新计算proc->time_slice
      * (3) 设置proc->rq指针为rq
      * (4) 增加rq->proc_num
      */
     /* 使用斜堆插入，传入修正后的比较函数 */
     rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
     
     /* 只有当时间片用完或未初始化时才重置，防止在此处频繁重置 */
     if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
          proc->time_slice = rq->max_time_slice;
     }
     
     proc->rq = rq;
     rq->proc_num++;
}

/*
 * stride_dequeue 从运行队列 ``rq'' 中移除进程 ``proc''，该操作将通过skew_heap_remove操作完成。
 * 记住更新 ``rq'' 结构。
 *
 * 提示: 参见 libs/skew_heap.h 中的优先队列结构例程。
 */
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc)
{
     /* LAB6 CHALLENGE 1: 2312145
      * (1) 正确地从rq移除proc
      * 注意: 你可以使用skew_heap或list。重要函数
      * skew_heap_remove: 从skew_heap移除条目
      * list_del_init: 从列表移除条目
      */
     rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
     rq->proc_num--;
}

/*
 * stride_pick_next 从 ``run-queue'' 中选择stride值最小的元素，并返回相应的进程指针。
 * 进程指针将通过宏le2proc计算，参见 kern/process/proc.h 中的定义。如果队列中没有进程，则返回NULL。
 *
 * 当选择一个proc结构时，记住更新proc的stride属性。(stride += BIG_STRIDE / priority)
 *
 * 提示: 参见 libs/skew_heap.h 中的优先队列结构例程。
 */
static struct proc_struct *
stride_pick_next(struct run_queue *rq)
{
     /* LAB6 CHALLENGE 1: 2312145
      * (1) 获取具有最小stride值的proc_struct指针p
      * (1.1) 如果使用skew_heap，可以使用le2proc从rq->lab6_run_pool获取p
      * (1.2) 如果使用list，则必须搜索列表以找到stride值最小的p
      * (2) 更新p的stride值: p->lab6_stride
      * (3) 返回p
      */
     if (rq->lab6_run_pool == NULL)
          return NULL;
     
     struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);
     
     /* 更新 stride，注意防止除零错误 (虽然 ucore 默认 priority=1) */
     if (p->lab6_priority == 0) {
          p->lab6_stride += BIG_STRIDE;
     } else {
          p->lab6_stride += BIG_STRIDE / p->lab6_priority;
     }
     
     return p;
}

/*
 * stride_proc_tick 与当前进程的tick事件一起工作。你应该检查当前进程的时间片是否耗尽，
 * 并更新proc结构体 ``proc''。proc->time_slice 表示当前进程剩余的时间片。
 * proc->need_resched 是进程切换的标志变量。
 */
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
     /* LAB6 CHALLENGE 1: 2312145 */
     if (proc->time_slice > 0) {
          proc->time_slice--;
     }
     if (proc->time_slice == 0) {
          proc->need_resched = 1;
     }
}

struct sched_class stride_sched_class = {
    .name = "stride_scheduler",
    .init = stride_init,
    .enqueue = stride_enqueue,
    .dequeue = stride_dequeue,
    .pick_next = stride_pick_next,
    .proc_tick = stride_proc_tick,
};