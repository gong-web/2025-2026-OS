/**
 * SLUB 内存分配器 - 实现文件
 * 学号：2312325 巩岱松
 * 日期：2025年10月11日
 * 
 * SLUB: 简化的 Unqueued Slab Allocator
 * 参考 Linux 内核 SLUB 分配器实现
 * 
 * 核心功能：
 * 1. 两层架构：PMM（页）+ SLUB（对象）
 * 2. 快速路径：CPU freelist 实现 O(1) 分配
 * 3. 慢速路径：partial 链表 + 按需 slab 分配
 * 4. 智能回收：空 slab 页自动返还给 PMM
 */

#include <slub.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <memlayout.h>

/* ============================================
 * 全局变量（学号：2312325）
 * ============================================ */

// 预定义的 8 个缓存池（16-2048 字节）
struct kmem_cache *slub_caches[SLUB_SIZE_COUNT];

// SLUB 初始化标志
int slub_initialized = 0;  // 使用 int 代替 bool (C99 兼容)

// 大小到索引的映射表
static const size_t size_table[SLUB_SIZE_COUNT] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};

// ============================================
// 内部工具函数：partial 链表与 CPU freelist 管理（学号：2312325）
// ============================================

static inline int page_on_partial(struct Page *page) {
    return page->page_link.next != NULL;
}

static void add_partial(struct kmem_cache *cache, struct Page *page) {
    if (page_on_partial(page)) {
        return;
    }
    list_add(&(cache->node.partial), &(page->page_link));
    cache->node.nr_partial++;
}

static void remove_partial(struct kmem_cache *cache, struct Page *page) {
    if (!page_on_partial(page)) {
        return;
    }
    list_del(&(page->page_link));
    page->page_link.prev = page->page_link.next = NULL;
    cache->node.nr_partial--;
}

static inline void cpu_push(struct kmem_cache *cache, void *object) {
    *(void **)object = cache->cpu.freelist;
    cache->cpu.freelist = object;
    cache->cpu.freelist_count++;
}

static inline void *cpu_pop(struct kmem_cache *cache) {
    void *object = cache->cpu.freelist;
    cache->cpu.freelist = *(void **)object;
    cache->cpu.freelist_count--;
    return object;
}

static void flush_cpu_freelist(struct kmem_cache *cache) {
    while (cache->cpu.freelist != NULL && cache->cpu.freelist_count > SLUB_CPU_LIMIT) {
        void *object = cpu_pop(cache);
        struct Page *page = virt_to_page(object);
        *(void **)object = page->freelist;
        page->freelist = object;
        if (page != cache->cpu.page && page->inuse < page->objects) {
            add_partial(cache, page);
        }
    }
}

static void drain_cpu_freelist(struct kmem_cache *cache) {
    while (cache->cpu.freelist != NULL) {
        void *object = cpu_pop(cache);
        struct Page *page = virt_to_page(object);
        *(void **)object = page->freelist;
        page->freelist = object;
        if (page != cache->cpu.page && page->inuse < page->objects) {
            add_partial(cache, page);
        }
    }
}

static struct Page *acquire_slab(struct kmem_cache *cache) {
    struct Page *page = cache->cpu.page;

    if (page != NULL && page->freelist != NULL) {
        return page;
    }

    if (page != NULL && page->freelist == NULL && cache->cpu.freelist == NULL && page->inuse == page->objects) {
        cache->cpu.page = NULL;
        page = NULL;
    }

    while (!list_empty(&cache->node.partial)) {
        list_entry_t *le = list_next(&cache->node.partial);
        struct Page *partial = le2page(le, page_link);
        remove_partial(cache, partial);
        cache->cpu.page = partial;
        return partial;
    }

    page = allocate_slab(cache);
    if (page != NULL) {
        cache->cpu.page = page;
    }
    return page;
}

static int refill_cpu_freelist(struct kmem_cache *cache) {
    struct Page *page = acquire_slab(cache);
    if (page == NULL) {
        return 0;
    }

    unsigned int batch = SLUB_CPU_BATCH;
    while (batch-- > 0 && page->freelist != NULL) {
        void *object = page->freelist;
        page->freelist = *(void **)object;
        cpu_push(cache, object);
    }

    return cache->cpu.freelist != NULL;
}

/* ============================================
 * 辅助函数实现（学号：2312325）
 * ============================================ */

/**
 * virt_to_page - 虚拟地址转 Page 结构
 * @addr:   虚拟地址
 * 
 * 返回值：对应的 Page 结构指针
 * 说明：假设线性映射（学号：2312325）
 */
struct Page *virt_to_page(void *addr) {
    if (addr == NULL) {
        return NULL;
    }
    uintptr_t va = (uintptr_t)addr;
    uintptr_t pa = PADDR(va);  // 虚拟地址转物理地址
    ppn_t ppn = pa >> PGSHIFT;  // 物理页号
    size_t page_index = ppn - nbase;  // 计算在 pages 数组中的索引（学号：2312325 - 修复）
    return &pages[page_index];
}

/**
 * page_to_virt - Page 结构转虚拟地址
 * @page:   Page 结构指针
 * 
 * 返回值：对应的虚拟地址
 * 说明：将页结构转换为内核虚拟地址（学号：2312325）
 */
void *page_to_virt(struct Page *page) {
    if (page == NULL) {
        return NULL;
    }
    // 使用 page2pa 获取物理地址，再加上偏移量得到虚拟地址（学号：2312325）
    uintptr_t pa = page2pa(page);
    return (void *)(pa + va_pa_offset);
}

/* get_cache_for_size - 根据大小选择合适的 cache
 */
struct kmem_cache *get_cache_for_size(size_t size) {
    for (int i = 0; i < SLUB_SIZE_COUNT; i++) {
        if (size <= size_table[i]) {
            return slub_caches[i];
        }
    }
    return NULL;  // 超过最大尺寸
}

/* init_slab_freelist - 初始化 slab 页的 freelist
 * 将页内存按对象大小切分，用链表串联所有对象
 */
void init_slab_freelist(struct kmem_cache *cache, struct Page *page) {
    void *addr = page_to_virt(page);
    size_t obj_size = cache->size;
    unsigned int objects = cache->objects_per_slab;
    
    // 设置页元数据
    page->s_mem = addr;
    page->inuse = 0;
    page->objects = objects;
    page->slab_cache = cache;
    page->flags |= PG_slab;
    
    // 构建 freelist：obj[0] -> obj[1] -> ... -> obj[N-1] -> NULL
    void *current = addr;
    for (unsigned int i = 0; i < objects - 1; i++) {
        void *next = (char *)current + obj_size;
        *(void **)current = next;
        current = next;
    }
    *(void **)current = NULL;  // 最后一个对象指向 NULL
    
    page->freelist = addr;  // 第一个对象
}

/* allocate_slab - 分配并初始化新的 slab 页
 * 学号：2312325 - 确保正确设置页面标志
 */
struct Page *allocate_slab(struct kmem_cache *cache) {
    // 从 PMM 分配一个页
    struct Page *page = alloc_page();
    if (page == NULL) {
        cprintf("SLUB: Failed to allocate page for slab\n");
        return NULL;
    }
    
    // 完全清除页面状态（学号：2312325）
    // PMM 分配的页面可能还有旧的 flags 和 property
    page->flags = 0;
    page->property = 0;
    page->ref = 0;
    
    // 清空链表指针（重要！）
    page->page_link.prev = NULL;
    page->page_link.next = NULL;
    
    // 设置 slab 标志
    page->flags = PG_slab;
    
    // 初始化 freelist
    init_slab_freelist(cache, page);
    
    cache->active_slabs++;
    
    return page;
}

/* free_slab - 释放 slab 页回 PMM
 * 学号：2312325 - 修复页面标志清理问题
 */
void free_slab(struct kmem_cache *cache, struct Page *page) {
    assert(page->inuse == 0);  // 确保页完全空闲
    
    // 完全重置页面状态以满足 PMM 的要求（学号：2312325）
    // PMM 的 free_pages 要求 !PageReserved && !PageProperty
    
    // 清除所有标志位
    page->flags = 0;
    ClearPageProperty(page);  // 确保 property 标志为 0
    page->property = 0;
    page->ref = 0;
    set_page_ref(page, 0);
    
    // 清理 SLUB 字段
    page->slab_cache = NULL;
    page->freelist = NULL;
    page->s_mem = NULL;
    page->inuse = 0;
    page->objects = 0;
    
    // 归还页面到 PMM
    free_page(page);
    cache->active_slabs--;
}

// =============================================================================
// 核心分配算法
// =============================================================================

/* kmem_cache_alloc - 分配对象（快速路径 + 慢速路径）
 */
void *kmem_cache_alloc(struct kmem_cache *cache) {
    if (cache == NULL) {
        return NULL;
    }
    
    cache->alloc_count++;
    
    if (cache->cpu.freelist == NULL) {
        if (!refill_cpu_freelist(cache)) {
            return NULL;
        }
    }

    void *object = cpu_pop(cache);
    struct Page *page = virt_to_page(object);
    page->inuse++;
    return object;
}

/* kmem_cache_free - 释放对象
 */
void kmem_cache_free(struct kmem_cache *cache, void *obj) {
    if (cache == NULL || obj == NULL) {
        return;
    }
    
    cache->free_count++;
    
    struct Page *page = virt_to_page(obj);
    
    // 简单检查：确保对象属于该页
    if (page->slab_cache != cache) {
        panic("SLUB: Freeing object to wrong cache");
    }
    
    cpu_push(cache, obj);
    page->inuse--;
    
    if (cache->cpu.freelist_count > SLUB_CPU_LIMIT) {
        flush_cpu_freelist(cache);
    }
}

/* kmem_cache_create - 创建新的缓存池
 */
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align, unsigned long flags, void (*ctor)(void *)) {
    struct kmem_cache *cache = (struct kmem_cache *)alloc_page(); // 简化：用一页存 cache 结构
    if (cache == NULL) {
        return NULL;
    }
    
    cache->name = name;
    cache->size = size;
    cache->align = align;
    
    // 计算每个 slab 的对象数
    // 简化：假设每个 slab 只有一页
    cache->objects_per_slab = (PGSIZE - sizeof(struct Page)) / size; // 粗略计算
    if (cache->objects_per_slab == 0) cache->objects_per_slab = 1;
    
    cache->cpu.freelist = NULL;
    cache->cpu.page = NULL;
    cache->cpu.freelist_count = 0;
    
    list_init(&cache->node.partial);
    cache->node.nr_partial = 0;
    
    cache->alloc_count = 0;
    cache->free_count = 0;
    cache->active_slabs = 0;
    
    return cache;
}

/* slub_init - 初始化 SLUB 系统
 */
void slub_init(void) {
    cprintf("SLUB: Initializing...\n");
    
    for (int i = 0; i < SLUB_SIZE_COUNT; i++) {
        // 简化：这里应该动态分配 cache 结构，但为了引导，我们可能需要静态分配或使用 bootmem
        // 这里假设我们已经有基本的内存分配能力（PMM）
        // 为了简单，我们使用预定义的数组，但需要初始化内容
        
        // 注意：在实际内核中，kmem_cache 自身也是从 SLUB 分配的（鸡生蛋问题）
        // 这里我们使用简单的静态数组或 PMM 分配来打破循环
        
        struct kmem_cache *cache = (struct kmem_cache *)alloc_page(); // 浪费一页存一个结构，但在初始化阶段可接受
        if (cache == NULL) {
            panic("SLUB: Failed to init cache structure");
        }
        
        cache->name = "slub_cache"; // 简化名称
        cache->size = size_table[i];
        cache->align = 8;
        cache->objects_per_slab = PGSIZE / size_table[i];
        
        cache->cpu.freelist = NULL;
        cache->cpu.page = NULL;
        cache->cpu.freelist_count = 0;
        list_init(&cache->node.partial);
        cache->node.nr_partial = 0;
        
        slub_caches[i] = cache;
    }
    
    slub_initialized = 1;
    cprintf("SLUB: Initialization complete.\n");
}

/* kmalloc - 通用内存分配接口
 */
void *kmalloc(size_t size) {
    if (!slub_initialized) {
        return NULL; 
    }
    
    struct kmem_cache *cache = get_cache_for_size(size);
    if (cache == NULL) {
        // 超过 2048 字节，回退到 PMM（按页分配）
        int order = 0;
        while ((1 << order) * PGSIZE < size) order++;
        struct Page *page = alloc_pages(1 << order);
        if (page == NULL) return NULL;
        return page2kva(page);
    }
    
    return kmem_cache_alloc(cache);
}

/* kfree - 通用内存释放接口
 */
void kfree(void *obj) {
    if (obj == NULL || !slub_initialized) {
        return;
    }
    
    struct Page *page = virt_to_page(obj);
    
    if (page->flags & PG_slab) {
        // 是 SLUB 管理的对象
        kmem_cache_free(page->slab_cache, obj);
    } else {
        // 是 PMM 管理的大页
        // 需要知道大小... 这里简化，假设是单页或需要记录大小
        // 在标准实现中，page->property 或其他字段会记录 order
        free_pages(page, 1); // 简化：假设只释放一页
    }
}

void kmalloc_init(void) {
    slub_init();
    cprintf("kmalloc_init() succeeded!\n");
}

size_t kallocated(void) {
    size_t total = 0;
    for (int i = 0; i < SLUB_SIZE_COUNT; i++) {
        if (slub_caches[i]) {
            total += (slub_caches[i]->alloc_count - slub_caches[i]->free_count) * slub_caches[i]->size;
        }
    }
    return total;
}
