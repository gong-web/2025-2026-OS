#include <default_pmm.h>
#include <defs.h>
#include <error.h>
#include <kmalloc.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <dtb.h>
#include <cow.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>
#include <riscv.h>

// virtual address of physical page array
struct Page *pages; // 物理页数组的虚拟基地址
// amount of physical memory (in pages)
size_t npage = 0; // 物理内存总页数
// The kernel image is mapped at VA=KERNBASE and PA=info.base
uint_t va_pa_offset; // 虚拟地址和物理地址的偏移量
// memory starts at 0x80000000 in RISC-V
const size_t nbase = DRAM_BASE / PGSIZE; // 物理内存起始页号

// virtual address of boot-time page directory
pde_t *boot_pgdir_va = NULL; // 启动页目录的虚拟地址
// physical address of boot-time page directory
uintptr_t boot_pgdir_pa; // 启动页目录的物理地址

// physical memory management
const struct pmm_manager *pmm_manager; // 物理内存管理器实例

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// init_pmm_manager - initialize a pmm_manager instance
// 初始化物理内存管理器
static void init_pmm_manager(void)
{
    pmm_manager = &default_pmm_manager; // 使用默认的物理内存管理器 (通常是 First Fit)
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
// 初始化内存映射，建立空闲页面的 Page 结构
static void init_memmap(struct Page *base, size_t n)
{
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
// 分配 n 个连续的物理页
struct Page *alloc_pages(size_t n)
{
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag); // 关中断，保证原子性
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag); // 开中断
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
// 释放 n 个连续的物理页
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag); // 关中断
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag); // 开中断
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
// 获取当前空闲页的总数
size_t nr_free_pages(void)
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

/* pmm_init - initialize the physical memory management */
/* pmm_init - 初始化物理内存管理系统 (detect memory, init pages, init pmm) */
static void page_init(void)
{
    extern char kern_entry[];

    va_pa_offset = PHYSICAL_MEMORY_OFFSET; // 设置虚拟/物理地址偏移

    uint64_t mem_begin = get_memory_base(); // 获取物理内存起始地址
    uint64_t mem_size = get_memory_size();  // 获取物理内存大小
    if (mem_size == 0)
    {
        panic("DTB memory info not available");
    }
    uint64_t mem_end = mem_begin + mem_size; // 计算物理内存结束地址

    cprintf("physcial memory map:\n");
    cprintf("  memory: 0x%08lx, [0x%08lx, 0x%08lx].\n", mem_size, mem_begin,
            mem_end - 1);

    uint64_t maxpa = mem_end;

    if (maxpa > KERNTOP) // 限制最大物理地址不超过 KERNTOP (虽然在64位下通常不会超)
    {
        maxpa = KERNTOP;
    }

    extern char end[]; // 内核结束地址 (由链接脚本定义)

    npage = maxpa / PGSIZE; // 计算总页数
    // BBL has put the initial page table at the first available page after the
    // kernel
    // so stay away from it by adding extra offset to end
    // pages 数组紧跟在内核代码/数据之后存放
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE);

    // 将所有页面初始化为保留状态
    for (size_t i = 0; i < npage - nbase; i++)
    {
        SetPageReserved(pages + i);
    }

    // 计算空闲内存的起始地址 (跳过 pages 数组占用的空间)
    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * (npage - nbase));

    mem_begin = ROUNDUP(freemem, PGSIZE);
    mem_end = ROUNDDOWN(mem_end, PGSIZE);
    if (freemem < mem_end)
    {
        // 将剩余的物理内存交给 pmm 管理
        init_memmap(pa2page(mem_begin), (mem_end - mem_begin) / PGSIZE);
    }
    cprintf("vapaofset is %llu\n", va_pa_offset);
}

// boot_map_segment - setup&enable the paging mechanism
// parameters
//  la:   linear address of this memory need to map (after x86 segment map)
//  size: memory size
//  pa:   physical address of this memory
//  perm: permission of this memory
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size,
                             uintptr_t pa, uint32_t perm)
{
    assert(PGOFF(la) == PGOFF(pa));
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;
    la = ROUNDDOWN(la, PGSIZE);
    pa = ROUNDDOWN(pa, PGSIZE);
    for (; n > 0; n--, la += PGSIZE, pa += PGSIZE)
    {
        pte_t *ptep = get_pte(pgdir, la, 1);
        assert(ptep != NULL);
        *ptep = pte_create(pa >> PGSHIFT, PTE_V | perm);
    }
}

// boot_alloc_page - allocate one page using pmm->alloc_pages(1)
// return value: the kernel virtual address of this allocated page
// note: this function is used to get the memory for PDT(Page Directory
// Table)&PT(Page Table)
static void *boot_alloc_page(void)
{
    struct Page *p = alloc_page();
    if (p == NULL)
    {
        panic("boot_alloc_page failed.\n");
    }
    return page2kva(p);
}

// pmm_init - setup a pmm to manage physical memory, build PDT&PT to setup
// paging mechanism
//         - check the correctness of pmm & paging mechanism, print PDT&PT
void pmm_init(void)
{
    // We need to alloc/free the physical memory (granularity is 4KB or other
    // size).
    // So a framework of physical memory manager (struct pmm_manager)is defined
    // in pmm.h
    // First we should init a physical memory manager(pmm) based on the
    // framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a
    // pmm
    check_alloc_page();

    // create boot_pgdir, an initial page directory(Page Directory Table, PDT)
    extern char boot_page_table_sv39[];
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
    boot_pgdir_pa = PADDR(boot_pgdir_va);

    check_pgdir();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // now the basic virtual memory map(see memalyout.h) is established.
    // check the correctness of the basic virtual memory map.
    check_boot_pgdir();

    kmalloc_init();
}

// get_pte - get pte and return the kernel virtual address of this pte for la
//        - if the PT contians this pte didn't exist, alloc a page for PT
// parameter:
//  pgdir:  the kernel virtual base address of PDT
//  la:     the linear address need to map
//  create: a logical value to decide if alloc a page for PT
// return vaule: the kernel virtual address of this pte
// get_pte - 获取线性地址 la 对应的页表项 (PTE) 的内核虚拟地址
// 如果对应的页表 (PT) 不存在，且 create 标志为 true，则分配一个新的物理页作为页表
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    // 获取一级页目录项 (PDX1)
    pde_t *pdep1 = &pgdir[PDX1(la)];
    if (!(*pdep1 & PTE_V)) // 如果一级页目录项无效 (不存在)
    {
        struct Page *page;
        // 如果不需要创建或者分配页面失败，返回 NULL
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1); // 设置页面引用计数
        uintptr_t pa = page2pa(page); // 获取物理地址
        memset(KADDR(pa), 0, PGSIZE); // 清空页面内容 (初始化为0)
        // 设置一级页目录项，指向新分配的页表 (作为二级页目录)
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }

    // 获取二级页目录项 (PDX0)
    // pdep1 指向的是二级页目录的物理页，需要先转为内核虚拟地址
    pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    if (!(*pdep0 & PTE_V)) // 如果二级页目录项无效
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        // 设置二级页目录项，指向新分配的页表 (作为最底层的页表)
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    // 返回三级页表项 (PTX) 的地址
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}

// get_page - get related Page struct for linear address la using PDT pgdir
// get_page - 根据页目录 pgdir 和线性地址 la，查找对应的 Page 结构体
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
    pte_t *ptep = get_pte(pgdir, la, 0); // 获取 PTE，不创建新页表
    if (ptep_store != NULL)
    {
        *ptep_store = ptep; // 如果需要，返回 PTE 的地址
    }
    if (ptep != NULL && *ptep & PTE_V) // 如果 PTE 存在且有效
    {
        return pte2page(*ptep); // 返回对应的 Page 结构体
    }
    return NULL;
}

// page_remove_pte - free an Page sturct which is related linear address la
//                - and clean(invalidate) pte which is related linear address la
// note: PT is changed, so the TLB need to be invalidate
// page_remove_pte - 移除线性地址 la 对应的页表项，并释放对应的物理页
static inline void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)
{
    if (*ptep & PTE_V) // 如果 PTE 有效
    {
        struct Page *page = pte2page(*ptep); // 获取对应的 Page 结构
        if (page_ref_dec(page) == 0) // 减少引用计数，如果为0则释放页面
        {
            free_page(page);
        }
        *ptep = 0; // 清空 PTE
        tlb_invalidate(pgdir, la); // 刷新 TLB
    }
}

// unmap_range - 移除指定范围 [start, end) 的虚拟内存映射
void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    do
    {
        pte_t *ptep = get_pte(pgdir, start, 0);
        if (ptep == NULL) // 如果中间某个页表不存在，跳过整个 PTSIZE 范围
        {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        if (*ptep != 0) // 如果 PTE 存在，移除映射
        {
            page_remove_pte(pgdir, start, ptep);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
}

// exit_range - 释放指定范围内的所有页表页 (用于进程退出时清理页表)
void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    uintptr_t d1start, d0start;
    int free_pt, free_pd0;
    pde_t *pd0, *pt, pde1, pde0;
    d1start = ROUNDDOWN(start, PDSIZE);
    d0start = ROUNDDOWN(start, PTSIZE);
    do
    {
        // level 1 page directory entry (一级页目录项)
        pde1 = pgdir[PDX1(d1start)];
        // if there is a valid entry, get into level 0
        // and try to free all page tables pointed to by
        // all valid entries in level 0 page directory,
        // then try to free this level 0 page directory
        // and update level 1 entry
        if (pde1 & PTE_V)
        {
            pd0 = page2kva(pde2page(pde1)); // 获取二级页目录的虚拟地址
            // try to free all page tables
            free_pd0 = 1;
            do
            {
                pde0 = pd0[PDX0(d0start)];
                if (pde0 & PTE_V)
                {
                    pt = page2kva(pde2page(pde0)); // 获取三级页表的虚拟地址
                    // try to free page table
                    free_pt = 1;
                    for (int i = 0; i < NPTEENTRY; i++)
                        if (pt[i] & PTE_V)
                        {
                            free_pt = 0; // 如果页表中还有有效项，则不能释放该页表
                            break;
                        }
                    // free it only when all entry are already invalid
                    if (free_pt)
                    {
                        free_page(pde2page(pde0)); // 释放页表页
                        pd0[PDX0(d0start)] = 0;    // 清空二级页目录项
                    }
                }
                else
                    free_pd0 = 0;
                d0start += PTSIZE;
            } while (d0start != 0 && d0start < d1start + PDSIZE && d0start < end);
            // free level 0 page directory only when all pde0s in it are already invalid
            if (free_pd0)
            {
                free_page(pde2page(pde1)); // 释放二级页目录页
                pgdir[PDX1(d1start)] = 0;  // 清空一级页目录项
            }
        }
        d1start += PDSIZE;
        d0start = d1start;
    } while (d1start != 0 && d1start < end);
}
/* copy_range - copy content of memory (start, end) of one process A to another
 * process B
 * @to:    the addr of process B's Page Directory
 * @from:  the addr of process A's Page Directory
 * @share: flags to indicate to dup OR share. We just use dup method, so it
 * didn't be used.
 *
 * CALL GRAPH: copy_mm-->dup_mmap-->copy_range
 *
 * copy_range - 将一个进程 A 的内存内容 (start 到 end) 复制给另一个进程 B
 * 通常用于 fork 操作。
 * @to:   目标进程 B 的页目录地址
 * @from: 源进程 A 的页目录地址
 * @share: 是否共享内存 (用于 COW 写时复制)
 */
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end,
               bool share)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));
    // copy content by page unit. (按页为单位复制)
    do
    {
        // call get_pte to find process A's pte according to the addr start
        // 获取源进程 A 在地址 start 处的 PTE
        pte_t *ptep = get_pte(from, start, 0), *nptep;
        if (ptep == NULL)
        {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        // call get_pte to find process B's pte according to the addr start. If
        // pte is NULL, just alloc a PT
        if (*ptep & PTE_V)
        {
            // 为目标进程 B 获取或创建对应的 PTE
            if ((nptep = get_pte(to, start, 1)) == NULL)
            {
                return -E_NO_MEM;
            }
            // 保留 A/D 位：如果在复制/重设映射时丢失 PTE_A，会导致后续取指/访存反复触发 page fault。
            uint32_t perm = (*ptep & (PTE_USER | PTE_A | PTE_D));
            // get page from ptep (获取源物理页)
            struct Page *page = pte2page(*ptep);
            assert(page != NULL);
            int ret = 0;
            
            if (share) {
                // [COW Implementation]
                // 写时复制 (Copy-On-Write) 实现:
                // 仅对“原本可写”的页面启用 COW：清除 PTE_W 并设置软件位 PTE_COW。
                // 对只读/可执行页面（如代码段），保持原权限共享，避免破坏可执行权限。
                if (perm & PTE_W) {
                    uint32_t cow_perm = (perm & (~PTE_W)) | PTE_COW;
                    page_insert(from, page, start, cow_perm);
                    ret = page_insert(to, page, start, cow_perm);
                } else {
                    ret = page_insert(to, page, start, perm);
                }
            } else {
                // 非共享模式 (深拷贝):
                // alloc a page for process B
                struct Page *npage = alloc_page();
                assert(npage != NULL);
                // 1. 获取源页和目标页的内核虚拟地址
                void *src_kvaddr = page2kva(page);
                void *dst_kvaddr = page2kva(npage);
                // 2. 复制内存内容
                memcpy(dst_kvaddr, src_kvaddr, PGSIZE);
                // 3. 建立映射
                ret = page_insert(to, npage, start, perm);
            }

            assert(ret == 0);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
    return 0;
}

// page_remove - free an Page which is related linear address la and has an
// validated pte
// page_remove - 移除线性地址 la 的映射，并释放相关资源
void page_remove(pde_t *pgdir, uintptr_t la)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep != NULL)
    {
        page_remove_pte(pgdir, la, ptep);
    }
}

// page_insert - build the map of phy addr of an Page with the linear addr la
// paramemters:
//  pgdir: the kernel virtual base address of PDT
//  page:  the Page which need to map
//  la:    the linear address need to map
//  perm:  the permission of this Page which is setted in related pte
// return value: always 0
// note: PT is changed, so the TLB need to be invalidate
// page_insert - 建立物理页 page 与线性地址 la 的映射关系
// 输入:
//   pgdir: 页目录基地址
//   page:  要映射的物理页结构
//   la:    线性地址 (虚拟地址)
//   perm:  权限标志
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    // 获取或创建 PTE
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
    {
        return -E_NO_MEM;
    }
    page_ref_inc(page); // 增加页面引用计数 (因为即将建立新的映射)
    if (*ptep & PTE_V) // 如果该地址原本已经有映射
    {
        struct Page *p = pte2page(*ptep);
        if (p == page) // 如果映射的是同一个页
        {
            page_ref_dec(page); // 引用计数减回去 (因为上面加了一次，实际上只是更新权限，引用数不变)
        }
        else // 如果映射的是不同的页
        {
            page_remove_pte(pgdir, la, ptep); // 先移除旧的映射
        }
    }
    // 设置 PTE: 物理页号 | 有效位 | 权限
    *ptep = pte_create(page2ppn(page), PTE_V | perm);
    tlb_invalidate(pgdir, la); // 刷新 TLB
    return 0;
}

// invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
// tlb_invalidate - 刷新指定线性地址 la 的 TLB 表项
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    asm volatile("sfence.vma %0" : : "r"(la)); // RISC-V 刷新 TLB 指令
}

// pgdir_alloc_page - call alloc_page & page_insert functions to
//                  - allocate a page size memory & setup an addr map
//                  - pa<->la with linear address la and the PDT pgdir
// pgdir_alloc_page - 分配一个新页并映射到指定线性地址 la
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm)
{
    struct Page *page = alloc_page(); // 分配物理页
    if (page != NULL)
    {
        if (page_insert(pgdir, page, la, perm) != 0) // 建立映射
        {
            free_page(page); // 失败则释放页面
            return NULL;
        }
        // swap_map_swappable(check_mm_struct, la, page, 0);
        page->pra_vaddr = la; // 记录该页对应的虚拟地址 (用于页面置换算法)
        assert(page_ref(page) == 1);
    }

    return page;
}

static void check_alloc_page(void)
{
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

static void check_pgdir(void)
{
    // assert(npage <= KMEMSIZE / PGSIZE);
    // The memory starts at 2GB in RISC-V
    // so npage is always larger than KMEMSIZE / PGSIZE
    size_t nr_free_store;

    nr_free_store = nr_free_pages();

    assert(npage <= KERNTOP / PGSIZE);
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0);
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL);

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);

    p2 = alloc_page();
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert(boot_pgdir_va[0] & PTE_U);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir_va, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir_va, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_pgdir() succeeded!\n");
}

static void check_boot_pgdir(void)
{
    size_t nr_free_store;
    pte_t *ptep;
    int i;

    nr_free_store = nr_free_pages();

    for (i = ROUNDDOWN(KERNBASE, PGSIZE); i < npage * PGSIZE; i += PGSIZE)
    {
        assert((ptep = get_pte(boot_pgdir_va, (uintptr_t)KADDR(i), 0)) != NULL);
        assert(PTE_ADDR(*ptep) == i);
    }

    assert(boot_pgdir_va[0] == 0);

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(p);
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_boot_pgdir() succeeded!\n");
}

// perm2str - use string 'u,r,w,-' to present the permission
static const char *perm2str(int perm)
{
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

// get_pgtable_items - In [left, right] range of PDT or PT, find a continuous
// linear addr space
//                  - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                  - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PGSIZE=4K, if PT
// paramemters:
//  left:        no use ???
//  right:       the high side of table's range
//  start:       the low side of table's range
//  table:       the beginning addr of table
//  left_store:  the pointer of the high side of table's next range
//  right_store: the pointer of the low side of table's next range
//  return value: 0 - not a invalid item range, perm - a valid item range with
//  perm permission
static int get_pgtable_items(size_t left, size_t right, size_t start,
                             uintptr_t *table, size_t *left_store,
                             size_t *right_store)
{
    if (start >= right)
    {
        return 0;
    }
    while (start < right && !(table[start] & PTE_V))
    {
        start++;
    }
    if (start < right)
    {
        if (left_store != NULL)
        {
            *left_store = start;
        }
        int perm = (table[start++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm)
        {
            start++;
        }
        if (right_store != NULL)
        {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}
