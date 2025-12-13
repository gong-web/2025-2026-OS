#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <atomic.h>
#include <assert.h>

// pmm_manager is a physical memory management class. A special pmm manager - XXX_pmm_manager
// only needs to implement the methods in pmm_manager class, then XXX_pmm_manager can be used
// by ucore to manage the total physical memory space.
// 物理内存管理器接口结构体
// 具体的物理内存管理器（如 first_fit_pmm_manager）只需要实现这个接口中的方法
// ucore 使用这个接口来统一管理物理内存
struct pmm_manager
{
    const char *name;                                 // XXX_pmm_manager's name (管理器名称)
    void (*init)(void);                               // initialize internal description&management data structure
                                                      // (free block list, number of free block) of XXX_pmm_manager
                                                      // 初始化内部数据结构（如空闲块链表、空闲块数量等）
    void (*init_memmap)(struct Page *base, size_t n); // setup description&management data structcure according to
                                                      // the initial free physical memory space
                                                      // 根据初始空闲物理内存空间设置管理数据结构
    struct Page *(*alloc_pages)(size_t n);            // allocate >=n pages, depend on the allocation algorithm
                                                      // 分配 >=n 个物理页，具体策略取决于分配算法
    void (*free_pages)(struct Page *base, size_t n);  // free >=n pages with "base" addr of Page descriptor structures(memlayout.h)
                                                      // 释放从 base 开始的 >=n 个物理页
    size_t (*nr_free_pages)(void);                    // return the number of free pages
                                                      // 返回当前空闲页的数量
    void (*check)(void);                              // check the correctness of XXX_pmm_manager
                                                      // 检查内存管理器的正确性（用于测试）
};

extern const struct pmm_manager *pmm_manager;
extern pde_t *boot_pgdir_va; // 启动页目录的虚拟地址
extern const size_t nbase;
extern uintptr_t boot_pgdir_pa; // 启动页目录的物理地址

void pmm_init(void);

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store);
void page_remove(pde_t *pgdir, uintptr_t la);
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);

void load_esp0(uintptr_t esp0);
void tlb_invalidate(pde_t *pgdir, uintptr_t la);
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm);
void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end);
void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end);
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share);

void print_pgdir(void);

/* *
 * PADDR - takes a kernel virtual address (an address that points above KERNBASE),
 * where the machine's maximum 256MB of physical memory is mapped and returns the
 * corresponding physical address.  It panics if you pass it a non-kernel virtual address.
 *
 * PADDR - 将内核虚拟地址转换为物理地址
 * 输入：内核虚拟地址 (必须在 KERNBASE 之上)
 * 输出：对应的物理地址
 * 注意：如果传入非内核虚拟地址会触发 panic
 * */
#define PADDR(kva)                                                 \
    ({                                                             \
        uintptr_t __m_kva = (uintptr_t)(kva);                      \
        if (__m_kva < KERNBASE)                                    \
        {                                                          \
            panic("PADDR called with invalid kva %08lx", __m_kva); \
        }                                                          \
        __m_kva - va_pa_offset;                                    \
    })

/* *
 * KADDR - takes a physical address and returns the corresponding kernel virtual
 * address. It panics if you pass an invalid physical address.
 *
 * KADDR - 将物理地址转换为内核虚拟地址
 * 输入：物理地址
 * 输出：对应的内核虚拟地址
 * 注意：如果传入无效物理地址（超出内存范围）会触发 panic
 * */
#define KADDR(pa)                                                \
    ({                                                           \
        uintptr_t __m_pa = (pa);                                 \
        size_t __m_ppn = PPN(__m_pa);                            \
        if (__m_ppn >= npage)                                    \
        {                                                        \
            panic("KADDR called with invalid pa %08lx", __m_pa); \
        }                                                        \
        (void *)(__m_pa + va_pa_offset);                         \
    })

extern struct Page *pages; // page 结构体数组的起始地址
extern size_t npage;       // 物理内存页总数
extern uint_t va_pa_offset; // 虚拟地址与物理地址的偏移量

// Page 指针 -> 物理页号 (PPN)
static inline ppn_t
page2ppn(struct Page *page)
{
    return page - pages + nbase;
}

// Page 指针 -> 物理地址 (PA)
static inline uintptr_t
page2pa(struct Page *page)
{
    return page2ppn(page) << PGSHIFT;
}

// 物理地址 (PA) -> Page 指针
static inline struct Page *
pa2page(uintptr_t pa)
{
    if (PPN(pa) >= npage)
    {
        panic("pa2page called with invalid pa");
    }
    return &pages[PPN(pa) - nbase];
}

// Page 指针 -> 内核虚拟地址 (KVA)
static inline void *
page2kva(struct Page *page)
{
    return KADDR(page2pa(page));
}

// 内核虚拟地址 (KVA) -> Page 指针
static inline struct Page *
kva2page(void *kva)
{
    return pa2page(PADDR(kva));
}

// 页表项 (PTE) -> Page 指针
static inline struct Page *
pte2page(pte_t pte)
{
    if (!(pte & PTE_V))
    {
        panic("pte2page called with invalid pte");
    }
    return pa2page(PTE_ADDR(pte));
}

// 页目录项 (PDE) -> Page 指针
static inline struct Page *
pde2page(pde_t pde)
{
    return pa2page(PDE_ADDR(pde));
}

// 获取页面引用计数
static inline int
page_ref(struct Page *page)
{
    return page->ref;
}

// 设置页面引用计数
static inline void
set_page_ref(struct Page *page, int val)
{
    page->ref = val;
}

// 增加页面引用计数 (原子操作)
static inline int
page_ref_inc(struct Page *page)
{
    return __sync_add_and_fetch(&page->ref, 1);
}

// 减少页面引用计数 (原子操作)
static inline int
page_ref_dec(struct Page *page)
{
    return __sync_sub_and_fetch(&page->ref, 1);
}

// 刷新 TLB (Translation Lookaside Buffer)
// sfence.vma 是 RISC-V 指令，用于刷新虚拟内存相关的缓存
static inline void flush_tlb()
{
    asm volatile("sfence.vma");
}

// construct PTE from a page and permission bits
// 根据物理页号和权限位构造 PTE
static inline pte_t pte_create(uintptr_t ppn, int type)
{
    return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
}

static inline pte_t ptd_create(uintptr_t ppn)
{
    return pte_create(ppn, PTE_V);
}

extern char bootstack[], bootstacktop[];

#endif /* !__KERN_MM_PMM_H__ */
