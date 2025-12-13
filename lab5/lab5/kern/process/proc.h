#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>

// process's state in his life cycle
// 进程生命周期中的状态
enum proc_state
{
    PROC_UNINIT = 0, // uninitialized (未初始化)
    PROC_SLEEPING,   // sleeping (睡眠/阻塞状态)
    PROC_RUNNABLE,   // runnable(maybe running) (就绪/运行状态)
    PROC_ZOMBIE,     // almost dead, and wait parent proc to reclaim his resource (僵尸状态: 已退出但资源未回收)
};

// 上下文结构体 (用于进程切换，保存被调用者保存寄存器)
struct context
{
    uintptr_t ra;  // 返回地址
    uintptr_t sp;  // 栈指针
    uintptr_t s0;  // 保存寄存器 s0-s11
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

#define PROC_NAME_LEN 15
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)

extern list_entry_t proc_list;

// 进程控制块 (PCB)
struct proc_struct
{
    enum proc_state state;                  // Process state (进程状态)
    int pid;                                // Process ID (进程 ID)
    int runs;                               // the running times of Proces (运行次数/时间片)
    uintptr_t kstack;                       // Process kernel stack (内核栈基址)
    volatile bool need_resched;             // bool value: need to be rescheduled to release CPU? (是否需要调度)
    struct proc_struct *parent;             // the parent process (父进程指针)
    struct mm_struct *mm;                   // Process's memory management field (内存管理描述符)
    struct context context;                 // Switch here to run process (进程上下文，用于 switch_to)
    struct trapframe *tf;                   // Trap frame for current interrupt (中断帧，保存用户态寄存器)
    uintptr_t pgdir;                        // the base addr of Page Directroy Table(PDT) (页目录基地址 - 物理地址)
    uint32_t flags;                         // Process flag (进程标志)
    char name[PROC_NAME_LEN + 1];           // Process name (进程名称)
    list_entry_t list_link;                 // Process link list (全部进程链表节点)
    list_entry_t hash_link;                 // Process hash list (PID 哈希表节点)
    int exit_code;                          // exit code (be sent to parent proc) (退出码)
    uint32_t wait_state;                    // waiting state (等待状态原因)
    struct proc_struct *cptr, *yptr, *optr; // relations between processes (进程关系: child, younger_sibling, older_sibling)
};

#define PF_EXITING 0x00000001 // getting shutdown (进程正在退出)

#define WT_CHILD (0x00000001 | WT_INTERRUPTED) // 等待子进程
#define WT_INTERRUPTED 0x80000000 // the wait state could be interrupted (等待可被中断)

#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_exit(int error_code);
int do_yield(void);
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size);
int do_wait(int pid, int *code_store);
int do_kill(int pid);
#endif /* !__KERN_PROCESS_PROC_H__ */
