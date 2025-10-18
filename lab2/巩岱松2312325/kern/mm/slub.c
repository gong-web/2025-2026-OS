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

    if (page->inuse == page->objects) {
        if (page_on_partial(page)) {
            remove_partial(cache, page);
        }
        if (page == cache->cpu.page && cache->cpu.freelist == NULL && page->freelist == NULL) {
            cache->cpu.page = NULL;
        }
    }

    return object;
}

/* kmem_cache_free - 释放对象
 * 学号：2312325 - 简化版本（不使用 partial 链表）
 */
void kmem_cache_free(struct kmem_cache *cache, void *object) {
    if (cache == NULL || object == NULL) {
        return;
    }
    
    cache->free_count++;
    
    // 找到对象所在的页（学号：2312325）
    struct Page *page = virt_to_page(object);
    
    assert(page->flags & PG_slab);
    assert(page->slab_cache == cache);
    
    page->inuse--;

    if (page == cache->cpu.page) {
        cpu_push(cache, object);
        flush_cpu_freelist(cache);

        if (page->inuse == 0 && cache->cpu.freelist_count == 0) {
            cache->cpu.page = NULL;
        }
    } else {
        *(void **)object = page->freelist;
        page->freelist = object;

        if (page->inuse == 0) {
            remove_partial(cache, page);
            free_slab(cache, page);
            return;
        }

        if (page->inuse < page->objects) {
            add_partial(cache, page);
        }
    }
}

// =============================================================================
// 缓存管理
// =============================================================================

/* kmem_cache_create - 创建新缓存
 */
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align) {
    if (size == 0 || size > PGSIZE) {
        return NULL;
    }
    
    struct kmem_cache *cache = (struct kmem_cache *)slub_alloc(sizeof(struct kmem_cache));
    if (cache == NULL) {
        // 初次调用可能需要手动分配
        struct Page *page = alloc_page();
        if (page == NULL) {
            return NULL;
        }
        cache = (struct kmem_cache *)page_to_virt(page);
    }
    
    memset(cache, 0, sizeof(struct kmem_cache));
    
    cache->name = name;
    cache->size = size;
    cache->align = (align == 0) ? sizeof(void *) : align;
    cache->objects_per_slab = PGSIZE / size;
    
    list_init(&cache->node.partial);
    cache->node.nr_partial = 0;
    
    cache->cpu.freelist = NULL;
    cache->cpu.page = NULL;
    cache->cpu.freelist_count = 0;
    
    return cache;
}

/* kmem_cache_destroy - 销毁缓存
 */
void kmem_cache_destroy(struct kmem_cache *cache) {
    if (cache == NULL) {
        return;
    }
    
    drain_cpu_freelist(cache);

    // 释放当前活动页
    if (cache->cpu.page != NULL) {
        free_slab(cache, cache->cpu.page);
    }
    
    // 释放 partial 链表中的所有页
    while (!list_empty(&cache->node.partial)) {
        list_entry_t *le = list_next(&cache->node.partial);
        struct Page *page = le2page(le, page_link);
        list_del(&page->page_link);
        free_slab(cache, page);
    }
    
    // 释放 cache 结构本身
    slub_free(cache);
}

// =============================================================================
// 通用分配接口
// =============================================================================

/* slub_alloc - 通用内存分配
 */
void *slub_alloc(size_t size) {
    if (!slub_initialized) {
        // 初始化阶段，直接从 PMM 分配
        if (size <= PGSIZE) {
            struct Page *page = alloc_page();
            return page ? page_to_virt(page) : NULL;
        }
        return NULL;
    }
    
    if (size == 0) {
        return NULL;
    }
    
    // 超大对象直接从 PMM 分配
    if (size > SLUB_MAX_SIZE) {
        size_t pages = (size + PGSIZE - 1) / PGSIZE;
        struct Page *page = alloc_pages(pages);
        return page ? page_to_virt(page) : NULL;
    }
    
    // 从合适的 cache 分配
    struct kmem_cache *cache = get_cache_for_size(size);
    if (cache == NULL) {
        return NULL;
    }
    
    return kmem_cache_alloc(cache);
}

/* slub_free - 通用内存释放
 */
void slub_free(void *obj) {
    if (obj == NULL) {
        return;
    }
    
    struct Page *page = virt_to_page(obj);
    
    // 检查是否是 slab 对象
    if (page->flags & PG_slab) {
        struct kmem_cache *cache = page->slab_cache;
        kmem_cache_free(cache, obj);
    } else {
        // 直接分配的页，归还到 PMM
        free_page(page);
    }
}

// =============================================================================
// 初始化
// =============================================================================

/* slub_init - 初始化 SLUB 系统
 */
void slub_init(void) {
    cprintf("SLUB: Initializing SLUB allocator...\n");
    
    // 创建预定义大小的缓存
    for (int i = 0; i < SLUB_SIZE_COUNT; i++) {
        char name[32];
        snprintf(name, sizeof(name), "kmalloc-%d", (int)size_table[i]);
        slub_caches[i] = kmem_cache_create(name, size_table[i], 0);
        
        if (slub_caches[i] == NULL) {
            panic("SLUB: Failed to create cache for size %d\n", (int)size_table[i]);
        }
        
        cprintf("  Created cache: %s (obj_size=%d, objects_per_slab=%d)\n",
                name, (int)size_table[i], slub_caches[i]->objects_per_slab);
    }
    
    slub_initialized = 1;  // 标记为已初始化（学号：2312325）
    cprintf("SLUB: Initialization complete\n");
}

// =============================================================================
// 调试和统计
// =============================================================================

/* slub_print_stats - 打印统计信息
 */
void slub_print_stats(void) {
    cprintf("\n========== SLUB Statistics ==========\n");
    cprintf("%-15s %8s %8s %8s %8s %8s\n", 
            "Cache", "Size", "Allocs", "Frees", "Active", "Partial");
    cprintf("-----------------------------------------------------\n");
    
    for (int i = 0; i < SLUB_SIZE_COUNT; i++) {
        struct kmem_cache *cache = slub_caches[i];
        if (cache != NULL) {
            cprintf("%-15s %8d %8lu %8lu %8lu %8lu\n",
                    cache->name,
                    (int)cache->size,
                    cache->alloc_count,
                    cache->free_count,
                    cache->active_slabs,
                    cache->node.nr_partial);
        }
    }
    cprintf("=====================================\n\n");
}

/* slub_check - 完整性检查
 */
void slub_check(void) {
    cprintf("SLUB: Running integrity check...\n");
    
    for (int i = 0; i < SLUB_SIZE_COUNT; i++) {
        struct kmem_cache *cache = slub_caches[i];
        if (cache == NULL) {
            continue;
        }
        
        // 检查 partial 链表一致性（学号：2312325）
        unsigned long count = 0;
        list_entry_t *le = &(cache->node.partial);
        // 遍历 partial 链表中的所有 slab 页
        while ((le = list_next(le)) != &(cache->node.partial)) {
            struct Page *page = le2page(le, page_link);
            assert(page->flags & PG_slab);
            assert(page->slab_cache == cache);
            assert(page->inuse > 0 && page->inuse < page->objects);
            count++;
        }
        assert(count == cache->node.nr_partial);
        
        cprintf("  Cache %s: OK\n", cache->name);
    }
    
    cprintf("SLUB: Integrity check passed\n");
}

// =============================================================================
// 测试用例将在单独文件实现
// =============================================================================
