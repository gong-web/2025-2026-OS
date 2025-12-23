// COW (Copy-on-Write) 机制的辅助函数和宏定义
// 用于增强 uCore 的 COW 实现和安全性

#ifndef __KERN_MM_COW_H__
#define __KERN_MM_COW_H__

#include <defs.h>
#include <mmu.h>
#include <pmm.h>

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

struct mm_struct;
struct vma_struct;

// ===== COW 状态定义 =====

// COW页面的标志位（使用PTE的保留位）
// 在RISC-V Sv32中，PTE的第8-9位是保留给软件使用的
#define PTE_COW    0x100    // 标记这是一个COW页面（使用bit 8）

// ===== COW 状态机 =====
/*
 * COW页面的状态转换：
 * 
 * [初始状态] NORMAL_RW
 *      |
 *      | fork() with share=1
 *      v
 * [共享状态] COW_SHARED (PTE_V=1, PTE_W=0, PTE_COW=1, ref_count>1)
 *      |
 *      | write access (page fault)
 *      v
 *      +---> ref_count > 1? ---YES---> [复制状态] COPYING
 *      |                                     |
 *      |                                     | alloc + memcpy
 *      |                                     v
 *      |                                [独占状态] EXCLUSIVE_RW (新页面)
 *      |
 *      +---> ref_count == 1? --YES---> [独占状态] EXCLUSIVE_RW (原页面，升级权限)
 *
 * 状态说明：
 * 1. NORMAL_RW: 普通的可读写页面（未共享）
 * 2. COW_SHARED: fork后的共享页面，标记为只读
 * 3. COPYING: 正在执行COW复制的中间状态
 * 4. EXCLUSIVE_RW: 独占访问的可读写页面
 */

// ===== COW 辅助函数 =====

// 检查PTE是否为COW页面
static inline bool is_cow_page(pte_t pte) {
    // COW页面的特征：有效 + 不可写 + COW标志
    return (pte & PTE_V) && !(pte & PTE_W) && (pte & PTE_COW);
}

// 设置PTE为COW模式
static inline pte_t set_cow_pte(pte_t pte) {
    // 移除写权限，添加COW标志
    return (pte & ~PTE_W) | PTE_COW;
}

// 清除PTE的COW标志，恢复写权限
static inline pte_t clear_cow_pte(pte_t pte) {
    // 移除COW标志，添加写权限
    return (pte & ~PTE_COW) | PTE_W;
}

// ===== COW 统计信息 =====
struct cow_stats {
    uint64_t cow_faults;        // COW页面错误次数
    uint64_t cow_copies;        // 实际复制的次数
    uint64_t cow_upgrades;      // 直接升级权限的次数（ref=1）
    uint64_t pages_saved;       // 通过COW节省的页面数
};

extern struct cow_stats global_cow_stats;

// 初始化COW统计
static inline void cow_stats_init(void) {
    global_cow_stats.cow_faults = 0;
    global_cow_stats.cow_copies = 0;
    global_cow_stats.cow_upgrades = 0;
    global_cow_stats.pages_saved = 0;
}

// 打印COW统计信息
void print_cow_stats(void);

// ===== COW 核心函数声明 =====

/**
 * 处理COW页面错误
 * @param mm: 内存管理结构
 * @param addr: 发生错误的地址
 * @param ptep: 页表项指针
 * @return: 0表示成功，其他表示错误码
 * 
 * 功能：
 * 1. 检查页面引用计数
 * 2. 如果ref>1，分配新页面并复制
 * 3. 如果ref=1，直接升级权限
 * 4. 更新TLB
 */
int handle_cow_fault(struct mm_struct *mm, uintptr_t addr, pte_t *ptep);

/**
 * 将页面设置为COW模式
 * @param pgdir: 页目录
 * @param la: 线性地址
 * @return: 0表示成功
 */
int make_page_cow(pde_t *pgdir, uintptr_t la);

/**
 * 检查并执行COW复制（如果需要）
 * @param mm: 内存管理结构
 * @param addr: 地址
 * @param write: 是否为写操作
 * @return: 0表示成功
 */
int check_and_do_cow(struct mm_struct *mm, uintptr_t addr, bool write);

// ===== Dirty COW 防御机制 =====

/**
 * 验证COW操作的安全性
 * @param mm: 内存管理结构
 * @param addr: 地址
 * @param vma: 虚拟内存区域
 * @return: true表示安全，false表示检测到攻击
 * 
 * 安全检查：
 * 1. VMA权限检查（必须有VM_WRITE）
 * 2. 进程权限检查
 * 3. 防止竞争条件
 */
bool validate_cow_security(struct mm_struct *mm, uintptr_t addr, struct vma_struct *vma);

/**
 * 防御Dirty COW类型的攻击
 * @param mm: 内存管理结构
 * @return: 检测到的攻击次数
 * 
 * 防御措施：
 * 1. 在COW操作期间持有锁
 * 2. 原子性检查和执行COW
 * 3. 验证VMA权限
 */
int defend_dirty_cow(struct mm_struct *mm);

// ===== COW 调试支持 =====

#ifdef COW_DEBUG
#define COW_LOG(fmt, ...) \
    cprintf("[COW] " fmt "\n", ##__VA_ARGS__)
#else
#define COW_LOG(fmt, ...)
#endif

// 调试：打印页面的COW状态
void print_page_cow_status(struct Page *page, uintptr_t addr);

// 调试：验证COW不变量
bool verify_cow_invariants(struct mm_struct *mm);

#endif /* __KERN_MM_COW_H__ */
