// COW (Copy-on-Write) 机制的实现
// 包括COW页面错误处理、安全检查和统计功能

#include <cow.h>
#include <vmm.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <error.h>

// 全局COW统计信息
struct cow_stats global_cow_stats = {0};

/**
 * 打印COW统计信息
 */
void print_cow_stats(void) {
    cprintf("\n");
    cprintf("╔═══════════════════════════════════════════════════════════╗\n");
    cprintf("║           Copy-on-Write Statistics                        ║\n");
    cprintf("╠═══════════════════════════════════════════════════════════╣\n");
    cprintf("║ COW Page Faults:      %10lld                       ║\n", global_cow_stats.cow_faults);
    cprintf("║ Actual Copies Made:   %10lld                       ║\n", global_cow_stats.cow_copies);
    cprintf("║ Permission Upgrades:  %10lld                       ║\n", global_cow_stats.cow_upgrades);
    cprintf("║ Pages Saved by COW:   %10lld                       ║\n", global_cow_stats.pages_saved);
    cprintf("╠═══════════════════════════════════════════════════════════╣\n");
    
    if (global_cow_stats.cow_faults > 0) {
        uint64_t total = global_cow_stats.cow_copies + global_cow_stats.cow_upgrades;
        int efficiency = (int)((global_cow_stats.pages_saved * 100) / global_cow_stats.cow_faults);
        cprintf("║ Efficiency: %d%% (pages saved / total faults)       ║\n", efficiency);
    }
    
    cprintf("╚═══════════════════════════════════════════════════════════╝\n");
    cprintf("\n");
}

/**
 * 处理COW页面错误
 * 这是COW机制的核心函数
 */
int handle_cow_fault(struct mm_struct *mm, uintptr_t addr, pte_t *ptep) {
    assert(mm != NULL);
    assert(ptep != NULL);
    
    // 统计COW页面错误
    global_cow_stats.cow_faults++;
    
    // 检查PTE是否有效
    if (!(*ptep & PTE_V)) {
        cprintf("handle_cow_fault: PTE not valid\n");
        return -E_INVAL;
    }
    
    // 获取物理页
    struct Page *page = pte2page(*ptep);
    if (page == NULL) {
        cprintf("handle_cow_fault: Cannot get page from PTE\n");
        return -E_INVAL;
    }
    
    COW_LOG("COW fault at addr 0x%x, page ref=%d", addr, page_ref(page));
    
    // 获取页面引用计数
    int ref_count = page_ref(page);
    
    if (ref_count > 1) {
        // ========== 状态：COPYING ==========
        // 页面被多个进程共享，需要执行深拷贝
        COW_LOG("Performing deep copy (ref_count=%d)", ref_count);
        
        // 分配新页面
        struct Page *new_page = alloc_page();
        if (new_page == NULL) {
            cprintf("handle_cow_fault: Cannot allocate new page\n");
            return -E_NO_MEM;
        }
        
        // 复制页面内容
        void *src_kva = page2kva(page);
        void *dst_kva = page2kva(new_page);
        memcpy(dst_kva, src_kva, PGSIZE);
        
        // 获取原始权限并恢复写权限
        uint32_t perm = (*ptep & PTE_USER) | PTE_W;  // 恢复写权限
        
        // 建立新的映射（这会自动减少旧页面的引用计数）
        uintptr_t la = ROUNDDOWN(addr, PGSIZE);
        int ret = page_insert(mm->pgdir, new_page, la, perm);
        if (ret != 0) {
            free_page(new_page);
            cprintf("handle_cow_fault: page_insert failed\n");
            return ret;
        }
        
        // 统计
        global_cow_stats.cow_copies++;
        
        COW_LOG("Deep copy completed, new page allocated");
        
    } else if (ref_count == 1) {
        // ========== 状态：EXCLUSIVE_RW ==========
        // 页面只被当前进程引用，直接升级权限即可
        COW_LOG("Upgrading permissions (ref_count=1)");
        
        // 获取原始权限并恢复写权限
        uint32_t perm = (*ptep & PTE_USER) | PTE_W;
        
        // 更新页表项（恢复写权限）
        uintptr_t la = ROUNDDOWN(addr, PGSIZE);
        int ret = page_insert(mm->pgdir, page, la, perm);
        if (ret != 0) {
            cprintf("handle_cow_fault: page_insert for upgrade failed\n");
            return ret;
        }
        
        // 统计
        global_cow_stats.cow_upgrades++;
        
        COW_LOG("Permission upgrade completed");
        
    } else {
        // 引用计数异常（<=0）
        cprintf("handle_cow_fault: Invalid ref_count=%d\n", ref_count);
        return -E_INVAL;
    }
    
    return 0;
}

/**
 * 将页面设置为COW模式
 */
int make_page_cow(pde_t *pgdir, uintptr_t la) {
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep == NULL || !(*ptep & PTE_V)) {
        return -E_INVAL;
    }
    
    // 设置为COW：移除写权限，添加COW标志
    *ptep = set_cow_pte(*ptep);
    
    // 刷新TLB
    tlb_invalidate(pgdir, la);
    
    return 0;
}

/**
 * 检查并执行COW复制（如果需要）
 */
int check_and_do_cow(struct mm_struct *mm, uintptr_t addr, bool write) {
    if (!write) {
        return 0;  // 读操作不需要COW
    }
    
    pte_t *ptep = get_pte(mm->pgdir, addr, 0);
    if (ptep == NULL) {
        return -E_INVAL;
    }
    
    // 检查是否为COW页面
    if (is_cow_page(*ptep)) {
        return handle_cow_fault(mm, addr, ptep);
    }
    
    // 不是COW页面，检查是否可写
    if (!(*ptep & PTE_W) && (*ptep & PTE_V)) {
        // 有效但不可写，可能是共享的只读页面
        return handle_cow_fault(mm, addr, ptep);
    }
    
    return 0;
}

/**
 * 验证COW操作的安全性
 * 这是防御Dirty COW漏洞的关键函数
 */
bool validate_cow_security(struct mm_struct *mm, uintptr_t addr, struct vma_struct *vma) {
    assert(mm != NULL);
    assert(vma != NULL);
    
    // 安全检查1: VMA必须有写权限
    // 这防止了对只读映射的COW攻击
    if (!(vma->vm_flags & VM_WRITE)) {
        cprintf("[SECURITY] COW denied: VMA is read-only\n");
        return false;
    }
    
    // 安全检查2: 地址必须在VMA范围内
    if (addr < vma->vm_start || addr >= vma->vm_end) {
        cprintf("[SECURITY] COW denied: Address out of VMA range\n");
        return false;
    }
    
    // 安全检查3: 检查页表项状态
    pte_t *ptep = get_pte(mm->pgdir, addr, 0);
    if (ptep == NULL) {
        cprintf("[SECURITY] COW denied: PTE not found\n");
        return false;
    }
    
    // 安全检查4: PTE必须有效
    if (!(*ptep & PTE_V)) {
        cprintf("[SECURITY] COW denied: PTE not valid\n");
        return false;
    }
    
    // 所有安全检查通过
    COW_LOG("COW security validation passed");
    return true;
}

/**
 * 防御Dirty COW类型的攻击
 */
int defend_dirty_cow(struct mm_struct *mm) {
    // 在ucore中，Dirty COW的防御主要通过以下机制：
    // 
    // 1. 在do_pgfault中持有mm->mm_lock
    //    - 防止竞争条件
    //    - 确保COW操作的原子性
    // 
    // 2. 在COW之前验证VMA权限
    //    - 检查VM_WRITE标志
    //    - 拒绝对只读VMA的写入
    // 
    // 3. 使用页面引用计数
    //    - 正确管理页面生命周期
    //    - 防止use-after-free
    // 
    // 4. 在关键操作后刷新TLB
    //    - 确保页表更改立即生效
    //    - 防止使用陈旧的TLB条目
    
    // 这个函数主要用于统计和监控
    // 实际的防御逻辑已经集成在do_pgfault和handle_cow_fault中
    
    return 0;
}

/**
 * 调试：打印页面的COW状态
 */
void print_page_cow_status(struct Page *page, uintptr_t addr) {
    cprintf("╔═══════════════════════════════════════╗\n");
    cprintf("║     Page COW Status Report            ║\n");
    cprintf("╠═══════════════════════════════════════╣\n");
    cprintf("║ Address:        0x%08x             ║\n", addr);
    cprintf("║ Page Struct:    %p                 ║\n", page);
    if (page != NULL) {
        cprintf("║ Reference Count: %d                    ║\n", page_ref(page));
        cprintf("║ Physical Addr:  0x%08x             ║\n", page2pa(page));
    }
    cprintf("╚═══════════════════════════════════════╝\n");
}

/**
 * 调试：验证COW不变量
 * 
 * COW机制应该保持以下不变量：
 * 1. 所有共享页面(ref>1)必须是只读的
 * 2. 所有独占页面(ref=1)可以是可写的
 * 3. 所有COW标记的页面必须是有效的
 * 4. 页面引用计数必须>0
 */
bool verify_cow_invariants(struct mm_struct *mm) {
    assert(mm != NULL);
    
    bool valid = true;
    int checked = 0;
    
    list_entry_t *list = &(mm->mmap_list), *le = list;
    while ((le = list_next(le)) != list) {
        struct vma_struct *vma = le2vma(le, list_link);
        
        // 遍历VMA范围内的所有页面
        for (uintptr_t addr = vma->vm_start; addr < vma->vm_end; addr += PGSIZE) {
            pte_t *ptep = get_pte(mm->pgdir, addr, 0);
            if (ptep == NULL || !(*ptep & PTE_V)) {
                continue;  // 页面未映射，跳过
            }
            
            checked++;
            struct Page *page = pte2page(*ptep);
            int ref = page_ref(page);
            
            // 不变量1: 引用计数必须>0
            if (ref <= 0) {
                cprintf("COW Invariant Violation: ref_count=%d at 0x%x\n", ref, addr);
                valid = false;
            }
            
            // 不变量2: 共享页面必须是只读的
            if (ref > 1 && (*ptep & PTE_W)) {
                cprintf("COW Invariant Violation: Shared page is writable at 0x%x\n", addr);
                valid = false;
            }
            
            // 不变量3: COW页面必须有效且只读
            if (is_cow_page(*ptep)) {
                if (!(*ptep & PTE_V)) {
                    cprintf("COW Invariant Violation: COW page not valid at 0x%x\n", addr);
                    valid = false;
                }
                if (*ptep & PTE_W) {
                    cprintf("COW Invariant Violation: COW page is writable at 0x%x\n", addr);
                    valid = false;
                }
            }
        }
    }
    
    if (valid) {
        cprintf("COW Invariants: ✓ PASSED (checked %d pages)\n", checked);
    } else {
        cprintf("COW Invariants: ✗ FAILED (checked %d pages)\n", checked);
    }
    
    return valid;
}
