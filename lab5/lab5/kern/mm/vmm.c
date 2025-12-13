#include <vmm.h>
#include <sync.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <error.h>
#include <pmm.h>
#include <riscv.h>
#include <kmalloc.h>
#include <sched.h>
#include <cow.h>

/*
  vmm design include two parts: mm_struct (mm) & vma_struct (vma)
  mm is the memory manager for the set of continuous virtual memory
  area which have the same PDT. vma is a continuous virtual memory area.
  There a linear link list for vma & a redblack link list for vma in mm.
---------------
  mm related functions:
   golbal functions
     struct mm_struct * mm_create(void)
     void mm_destroy(struct mm_struct *mm)
     int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)
--------------
  vma related functions:
   global functions
     struct vma_struct * vma_create (uintptr_t vm_start, uintptr_t vm_end,...)
     void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
     struct vma_struct * find_vma(struct mm_struct *mm, uintptr_t addr)
   local functions
     inline void check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
---------------
   check correctness functions
     void check_vmm(void);
     void check_vma_struct(void);
     void check_pgfault(void);
*/

static void check_vmm(void);
static void check_vma_struct(void);

// mm_create -  alloc a mm_struct & initialize it.
// mm_create - 分配并初始化一个 mm_struct
struct mm_struct *
mm_create(void)
{
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));

    if (mm != NULL)
    {
        list_init(&(mm->mmap_list)); // 初始化 VMA 链表
        mm->mmap_cache = NULL;       // 初始化缓存
        mm->pgdir = NULL;            // 页目录暂时为空
        mm->map_count = 0;           // VMA 计数为 0

        mm->sm_priv = NULL;

        set_mm_count(mm, 0);         // 引用计数初始化
        lock_init(&(mm->mm_lock));   // 初始化互斥锁
    }
    return mm;
}

// vma_create - alloc a vma_struct & initialize it. (addr range: vm_start~vm_end)
// vma_create - 分配并初始化一个 vma_struct (地址范围: [vm_start, vm_end))
struct vma_struct *
vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags)
{
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));

    if (vma != NULL)
    {
        vma->vm_start = vm_start;
        vma->vm_end = vm_end;
        vma->vm_flags = vm_flags;
    }
    return vma;
}

// find_vma - find a vma  (vma->vm_start <= addr <= vma_vm_end)
// find_vma - 查找包含地址 addr 的 VMA (满足 vma->vm_start <= addr < vma->vm_end)
// 如果没找到包含的，则返回第一个起始地址大于 addr 的 VMA (即 addr 之后的第一个 VMA)
struct vma_struct *
find_vma(struct mm_struct *mm, uintptr_t addr)
{
    struct vma_struct *vma = NULL;
    if (mm != NULL)
    {
        vma = mm->mmap_cache; // 先查缓存
        // 如果缓存不为空，且 addr 在缓存的 VMA 范围内，直接返回
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr))
        {
            bool found = 0;
            list_entry_t *list = &(mm->mmap_list), *le = list;
            // 遍历链表
            while ((le = list_next(le)) != list)
            {
                vma = le2vma(le, list_link);
                // 这里的逻辑其实是：找到第一个结束地址大于 addr 的 VMA ??
                // 不，标准实现通常是找到满足 vma->vm_end > addr 的第一个 VMA。
                // 但这里的代码逻辑似乎是：
                // 只要 vma->vm_start <= addr < vma->vm_end 就找到了。
                // 让我们仔细看下 list 是按 vm_start 排序的。
                // 这里的 while 循环似乎是在找包含 addr 的 VMA。
                if (vma->vm_start <= addr && addr < vma->vm_end)
                {
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                vma = NULL;
            }
        }
        if (vma != NULL)
        {
            mm->mmap_cache = vma; // 更新缓存
        }
    }
    return vma;
}

// check_vma_overlap - check if vma1 overlaps vma2 ?
// check_vma_overlap - 检查两个 VMA 是否重叠 (用于调试断言)
static inline void
check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
{
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start); // 前一个的结束必须小于等于后一个的开始
    assert(next->vm_start < next->vm_end);
}

// insert_vma_struct -insert vma in mm's list link
// insert_vma_struct - 将 vma 插入到 mm 的链表中 (保持按地址排序)
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
{
    assert(vma->vm_start < vma->vm_end);
    list_entry_t *list = &(mm->mmap_list);
    list_entry_t *le_prev = list, *le_next;

    list_entry_t *le = list;
    // 寻找插入位置
    while ((le = list_next(le)) != list)
    {
        struct vma_struct *mmap_prev = le2vma(le, list_link);
        if (mmap_prev->vm_start > vma->vm_start)
        {
            break;
        }
        le_prev = le;
    }

    le_next = list_next(le_prev);

    /* check overlap */
    // 检查是否与前一个或后一个 VMA 重叠
    if (le_prev != list)
    {
        check_vma_overlap(le2vma(le_prev, list_link), vma);
    }
    if (le_next != list)
    {
        check_vma_overlap(vma, le2vma(le_next, list_link));
    }

    vma->vm_mm = mm;
    list_add_after(le_prev, &(vma->list_link)); // 插入链表

    mm->map_count++;
}

// mm_destroy - free mm and mm internal fields
// mm_destroy - 销毁 mm_struct 及其挂载的所有 VMA
void mm_destroy(struct mm_struct *mm)
{
    assert(mm_count(mm) == 0); // 确引用计数为 0

    list_entry_t *list = &(mm->mmap_list), *le;
    while ((le = list_next(list)) != list)
    {
        list_del(le);
        kfree(le2vma(le, list_link)); // kfree vma (释放 VMA 结构体内存)
    }
    kfree(mm); // kfree mm (释放 mm 结构体内存)
    mm = NULL;
}

// mm_map - 建立虚拟地址映射 (实际上就是创建并插入 VMA)
int mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
           struct vma_struct **vma_store)
{
    uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
    if (!USER_ACCESS(start, end))
    {
        return -E_INVAL;
    }

    assert(mm != NULL);

    int ret = -E_INVAL;

    struct vma_struct *vma;
    // 检查是否与现有 VMA 重叠
    if ((vma = find_vma(mm, start)) != NULL && end > vma->vm_start)
    {
        goto out;
    }
    ret = -E_NO_MEM;

    // 创建新 VMA
    if ((vma = vma_create(start, end, vm_flags)) == NULL)
    {
        goto out;
    }
    // 插入 VMA
    insert_vma_struct(mm, vma);
    if (vma_store != NULL)
    {
        *vma_store = vma;
    }
    ret = 0;

out:
    return ret;
}

// dup_mmap - 复制内存映射 (用于 fork)
// 从 from 复制所有的 VMA 到 to，并复制页表内容
int dup_mmap(struct mm_struct *to, struct mm_struct *from)
{
    assert(to != NULL && from != NULL);
    list_entry_t *list = &(from->mmap_list), *le = list;
    // 遍历 from 的所有 VMA
    while ((le = list_prev(le)) != list)
    {
        struct vma_struct *vma, *nvma;
        vma = le2vma(le, list_link);
        // 为 to 创建相同的 VMA
        nvma = vma_create(vma->vm_start, vma->vm_end, vma->vm_flags);
        if (nvma == NULL)
        {
            return -E_NO_MEM;
        }

        insert_vma_struct(to, nvma);

        bool share = 1;
        // 复制页表内容 (copy_range 会处理 COW)
        if (copy_range(to->pgdir, from->pgdir, vma->vm_start, vma->vm_end, share) != 0)
        {
            return -E_NO_MEM;
        }
    }
    return 0;
}

// exit_mmap - 退出内存映射 (用于进程退出)
// 释放页表映射和物理页
void exit_mmap(struct mm_struct *mm)
{
    assert(mm != NULL && mm_count(mm) == 0);
    pde_t *pgdir = mm->pgdir;
    list_entry_t *list = &(mm->mmap_list), *le = list;
    // 第一遍遍历：解除映射 (unmap_range)
    while ((le = list_next(le)) != list)
    {
        struct vma_struct *vma = le2vma(le, list_link);
        unmap_range(pgdir, vma->vm_start, vma->vm_end);
    }
    // 第二遍遍历：释放页表 (exit_range)
    while ((le = list_next(le)) != list)
    {
        struct vma_struct *vma = le2vma(le, list_link);
        exit_range(pgdir, vma->vm_start, vma->vm_end);
    }
}

bool copy_from_user(struct mm_struct *mm, void *dst, const void *src, size_t len, bool writable)
{
    if (!user_mem_check(mm, (uintptr_t)src, len, writable))
    {
        return 0;
    }
    memcpy(dst, src, len);
    return 1;
}

bool copy_to_user(struct mm_struct *mm, void *dst, const void *src, size_t len)
{
    if (!user_mem_check(mm, (uintptr_t)dst, len, 1))
    {
        return 0;
    }
    memcpy(dst, src, len);
    return 1;
}

// vmm_init - initialize virtual memory management
//          - now just call check_vmm to check correctness of vmm
// vmm_init - 初始化虚拟内存管理 (目前仅运行检查)
void vmm_init(void)
{
    cow_stats_init();  // 初始化COW统计
    check_vmm();
}

int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;
    // 查找包含该地址的 VMA
    struct vma_struct *vma = find_vma(mm, addr);

    pgfault_num++;
    // 如果没有找到 VMA 或者地址超出 VMA 范围 (理论上 find_vma 已经保证了 start <= addr，但需检查 addr < end)
    // find_vma 返回的是满足 vma->vm_start <= addr < vma->vm_end 的 VMA。
    // 如果返回 NULL，说明该地址不属于任何 VMA。
    if (vma == NULL || vma->vm_start > addr) {
        cprintf("not valid addr %x, and  can not find it in vma\n", addr);
        goto failed;
    }
    
    // error_code mapping for RISC-V (passed as cause)
    // CAUSE_STORE_PAGE_FAULT = 15
    // 判断是否是写异常
    bool write = (error_code == CAUSE_STORE_PAGE_FAULT);
    /* Protect the COW handling by serializing mm modifications.
     * Note: do_pgfault runs in trap context; avoid blocking calls that
     * may call schedule(). Use try_lock spinning to acquire mm_lock. */
    // 使用自旋锁保护 mm 结构，防止多核竞争 (尽管目前 ucore 主要是单核，但这是一个好习惯)
    lock_mm(mm);

    // [Security Check] Dirty COW Protection
    // We must check the VMA permissions (vma->vm_flags) to ensure the process
    // is actually allowed to write to this address.
    // [安全检查] 权限检查
    // 如果是写异常，但 VMA 没有写权限，则是非法访问
    if (write && !(vma->vm_flags & VM_WRITE)) {
        cprintf("do_pgfault failed: write fault, but vma not writable\n");
        unlock_mm(mm);
        goto failed;
    }
    // cprintf("PF: %x\n", addr);
    
    // 设置页表项权限
    uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE) {
        perm |= (PTE_R | PTE_W);
    }
    if (vma->vm_flags & VM_READ) {
        perm |= PTE_R;
    }
    if (vma->vm_flags & VM_EXEC) {
        perm |= PTE_X;
    }

    // RISC-V Sv39/Sv32: A/D 位可能需要由软件维护。
    // 若缺失 PTE_A（以及可写页的 PTE_D），QEMU 可能会在映射后仍反复触发 page fault。
    perm |= PTE_A;
    if (perm & PTE_W) {
        perm |= PTE_D;
    }
    addr = ROUNDDOWN(addr, PGSIZE);

    ret = -E_NO_MEM;

    pte_t *ptep = NULL;
    
    // 获取 PTE，如果不存在则创建页表
    if ((ptep = get_pte(mm->pgdir, addr, 1)) == NULL) {
        cprintf("get_pte in do_pgfault failed\n");
        unlock_mm(mm);
        goto failed;
    }
    
    // 如果 PTE 全为 0 (即页面未映射)
    if (*ptep == 0) { 
        // 分配物理页并建立映射 (Demand Paging / 按需分配)
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            unlock_mm(mm);
            goto failed;
        }
    } else {
        // [COW Implementation]
        // If the PTE is valid (*ptep & PTE_V) but not writable (!(*ptep & PTE_W)),
        // and it's a write fault (write == true), then it's a Copy-On-Write case.
        // Note: We already checked vma->vm_flags & VM_WRITE above, so we know
        // the process *should* be able to write. The fact that PTE is RO means
        // it's a shared COW page.
        // [COW 写时复制实现]
        // 如果 PTE 有效且不可写，但是发生了写异常 (write=true)，且 VMA 允许写 (上面已检查)
        // 说明这是一个共享的 COW 页面。
        if (write && (*ptep & PTE_V) && !(*ptep & PTE_W)) {
             // 只有被 copy_range 标记为 COW 的只读页，才允许进入写时复制流程。
             // 对于真正的只读映射（例如代码段/rodata），应直接失败。
             if (!(*ptep & PTE_COW)) {
                 cprintf("do_pgfault failed: write fault on non-COW RO page\n");
                 unlock_mm(mm);
                 goto failed;
             }
             struct Page *page = pte2page(*ptep);
             // 如果页面被多个进程引用 (引用计数 > 1)
             if (page_ref(page) > 1) {
                 // 分配一个新的物理页
                 struct Page *npage = alloc_page();
                 if (npage == NULL) { unlock_mm(mm); goto failed; }
                 
                 // 复制原页面的内容到新页面
                 void * src_kvaddr = page2kva(page);
                 void * dst_kvaddr = page2kva(npage);
                 memcpy(dst_kvaddr, src_kvaddr, PGSIZE);
                 
                 // 建立新页面的映射 (赋予写权限)
                 // 注意：page_insert 会自动减少原页面的引用计数
                 if (page_insert(mm->pgdir, npage, addr, perm) != 0) {
                     free_page(npage);
                    unlock_mm(mm);
                    goto failed;
                 }
             } else {
                // 如果页面引用计数为 1 (只被当前进程引用，或者是最后一个引用的进程)
                // 直接修改权限为可写即可，不需要复制
                page_insert(mm->pgdir, page, addr, perm);
             }
        }
    }
    ret = 0;
    unlock_mm(mm);
failed:
    return ret;
}

// check_vmm - check correctness of vmm
static void
check_vmm(void)
{
    // size_t nr_free_pages_store = nr_free_pages();

    check_vma_struct();
    // check_pgfault();

    cprintf("check_vmm() succeeded.\n");
}

static void
check_vma_struct(void)
{
    // size_t nr_free_pages_store = nr_free_pages();

    struct mm_struct *mm = mm_create();
    assert(mm != NULL);

    int step1 = 10, step2 = step1 * 10;

    int i;
    for (i = step1; i >= 1; i--)
    {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    for (i = step1 + 1; i <= step2; i++)
    {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    list_entry_t *le = list_next(&(mm->mmap_list));

    for (i = 1; i <= step2; i++)
    {
        assert(le != &(mm->mmap_list));
        struct vma_struct *mmap = le2vma(le, list_link);
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
        le = list_next(le);
    }

    for (i = 5; i <= 5 * step2; i += 5)
    {
        struct vma_struct *vma1 = find_vma(mm, i);
        assert(vma1 != NULL);
        struct vma_struct *vma2 = find_vma(mm, i + 1);
        assert(vma2 != NULL);
        struct vma_struct *vma3 = find_vma(mm, i + 2);
        assert(vma3 == NULL);
        struct vma_struct *vma4 = find_vma(mm, i + 3);
        assert(vma4 == NULL);
        struct vma_struct *vma5 = find_vma(mm, i + 4);
        assert(vma5 == NULL);

        assert(vma1->vm_start == i && vma1->vm_end == i + 2);
        assert(vma2->vm_start == i && vma2->vm_end == i + 2);
    }

    for (i = 4; i >= 0; i--)
    {
        struct vma_struct *vma_below_5 = find_vma(mm, i);
        if (vma_below_5 != NULL)
        {
            cprintf("vma_below_5: i %x, start %x, end %x\n", i, vma_below_5->vm_start, vma_below_5->vm_end);
        }
        assert(vma_below_5 == NULL);
    }

    mm_destroy(mm);

    cprintf("check_vma_struct() succeeded!\n");
}
bool user_mem_check(struct mm_struct *mm, uintptr_t addr, size_t len, bool write)
{
    if (mm != NULL)
    {
        if (!USER_ACCESS(addr, addr + len))
        {
            return 0;
        }
        struct vma_struct *vma;
        uintptr_t start = addr, end = addr + len;
        while (start < end)
        {
            if ((vma = find_vma(mm, start)) == NULL || start < vma->vm_start)
            {
                return 0;
            }
            if (!(vma->vm_flags & ((write) ? VM_WRITE : VM_READ)))
            {
                return 0;
            }
            if (write && (vma->vm_flags & VM_STACK))
            {
                if (start < vma->vm_start + PGSIZE)
                { // check stack start & size
                    return 0;
                }
            }
            start = vma->vm_end;
        }
        return 1;
    }
    return KERN_ACCESS(addr, addr + len);
}

volatile unsigned int pgfault_num = 0;