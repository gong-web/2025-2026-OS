/**
 * SLUB 内存分配器 - 头文件
 * 学号：2312325 巩岱松
 * 日期：2025年10月11日
 * 
 * SLUB: 简化的 Unqueued Slab Allocator
 * 参考 Linux 内核 SLUB 分配器实现
 * 
 * 两层架构的内存分配器：
 * - 第一层：基于页的内存分配（PMM）
 * - 第二层：页内的对象分配（SLUB）
 */

#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>
#include <list.h>
#include <pmm.h>

/* ============================================
 * 预定义常量（学号：2312325）
 * ============================================ */

// 支持的对象大小范围（字节）
#define SLUB_MIN_SIZE       16      // 最小对象：16 字节
#define SLUB_MAX_SIZE       2048    // 最大对象：2048 字节
#define SLUB_SIZE_COUNT     8       // 预定义大小数量

// CPU 本地缓存批处理参数（学号：2312325 - 参考 Linux SLUB）
#define SLUB_CPU_BATCH      8       // 每次批量迁移的对象数量
#define SLUB_CPU_LIMIT      (SLUB_CPU_BATCH * 4)  // CPU freelist 上限，防止过度占用

// 每种大小对应的缓存索引
enum {
    SLUB_16   = 0,  // 16 字节
    SLUB_32   = 1,  // 32 字节
    SLUB_64   = 2,  // 64 字节
    SLUB_128  = 3,  // 128 字节
    SLUB_256  = 4,  // 256 字节
    SLUB_512  = 5,  // 512 字节
    SLUB_1024 = 6,  // 1024 字节
    SLUB_2048 = 7,  // 2048 字节
};

// Slab 页标志位（使用 Page->flags 的高位）
#define PG_slab             0x0100  // 标记该页为 slab 页

/* ============================================
 * 核心数据结构（学号：2312325）
 * ============================================ */

/**
 * kmem_cache_node: 每个 cache 的节点管理结构
 * 说明：简化实现，只有一个节点（不支持 NUMA）
 */
struct kmem_cache_node {
    struct list_entry partial;      // 部分使用的 slab 页链表
    unsigned long nr_partial;       // partial 链表中的页数
};

/**
 * kmem_cache_cpu: 每个 cache 的 CPU 本地结构
 * 说明：简化实现，假设单 CPU 环境
 * 作用：实现快速路径分配，避免加锁
 */
struct kmem_cache_cpu {
    void *freelist;                 // 指向下一个空闲对象（快速路径）
    struct Page *page;              // 当前正在使用的 slab 页
    unsigned int freelist_count;    // CPU freelist 中缓存的对象数
};

/**
 * kmem_cache: 内存对象缓存
 * 说明：管理特定大小对象的分配池
 * 学号：2312325
 */
struct kmem_cache {
    const char *name;               // 缓存名称（如 "kmalloc-64"）
    size_t size;                    // 对象大小（字节）
    size_t align;                   // 对齐要求（字节，通常为 8）
    unsigned int objects_per_slab;  // 每个 slab 页包含的对象数
    
    struct kmem_cache_cpu cpu;      // CPU 本地缓存（快速路径）
    struct kmem_cache_node node;    // 节点管理（partial 链表）
    
    // 统计信息（学号：2312325）
    unsigned long alloc_count;      // 总分配次数
    unsigned long free_count;       // 总释放次数
    unsigned long active_slabs;     // 活动 slab 页数量
};

/**
 * 扩展 Page 结构以支持 SLUB（学号：2312325）
 * 
 * 注意：以下字段需要添加到 memlayout.h 的 struct Page 中
 * 
 * struct Page {
 *     ...原有字段...
 *     
 *     // SLUB 字段（学号：2312325 添加）
 *     void *s_mem;                   // slab 中第一个对象的地址
 *     void *freelist;                // 空闲对象链表头
 *     unsigned short inuse;          // 已分配对象数量
 *     unsigned short objects;        // slab 中总对象数
 *     struct kmem_cache *slab_cache; // 所属的 kmem_cache
 * };
 */

/* ============================================
 * 核心接口（学号：2312325）
 * ============================================ */
// =============================================================================

/**
 * slub_init - 初始化 SLUB 分配器
 * 说明：创建 8 个预定义大小的缓存池（16-2048 字节）
 * 调用时机：在 pmm_init() 之后
 * 学号：2312325
 */
void slub_init(void);

/**
 * kmem_cache_create - 创建新的对象缓存
 * @name:   缓存名称（用于调试和统计）
 * @size:   对象大小（字节）
 * @align:  对齐要求（字节），0 表示使用默认对齐（8 字节）
 * 
 * 返回值：新创建的 kmem_cache 指针，失败返回 NULL
 * 学号：2312325
 */
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align);

/**
 * kmem_cache_alloc - 从缓存分配一个对象
 * @cache:  目标缓存
 * 
 * 返回值：对象指针，失败返回 NULL
 * 
 * 分配策略（学号：2312325）：
 * 1. 快速路径：直接从 cpu.freelist 获取（O(1)）
 * 2. 慢速路径：从当前 slab 页或 partial 链表获取
 * 3. 新页分配：如果无可用对象，分配新 slab 页
 */
void *kmem_cache_alloc(struct kmem_cache *cache);

/**
 * kmem_cache_free - 释放对象到缓存
 * @cache:  目标缓存
 * @object: 要释放的对象指针
 * 
 * 说明：对象归还到所属 slab 页的 freelist
 * 学号：2312325
 */
void kmem_cache_free(struct kmem_cache *cache, void *object);

/**
 * kmem_cache_destroy - 销毁缓存
 * @cache:  要销毁的缓存
 * 
 * 说明：释放所有 slab 页和缓存结构本身
 * 警告：销毁前确保所有对象已释放
 * 学号：2312325
 */
void kmem_cache_destroy(struct kmem_cache *cache);

/* ============================================
 * 通用分配接口（学号：2312325）
 * 类似 Linux 的 kmalloc/kfree
 * ============================================ */

/**
 * slub_alloc - 分配任意大小的内存块
 * @size:   请求的字节数
 * 
 * 返回：内存块指针，失败返回 NULL
 * 
 * 自动选择合适大小的 cache：
 * - size <= 16    -> SLUB_16
 * - size <= 32    -> SLUB_32
 * - ...
 * - size <= 2048  -> SLUB_2048
 * - size > 2048   -> 直接从 PMM 分配页
 */
void *slub_alloc(size_t size);

/* slub_free - 释放内存块
 * @obj:    要释放的内存指针
 * 
 * 自动识别对象来源：
 * - 如果是 slab 对象，归还到对应 cache
 * - 如果是直接分配的页，归还到 PMM
 */
void slub_free(void *obj);

// =============================================================================
// 调试和测试接口
// =============================================================================

/* slub_check - 完整性检查
 * 验证所有 cache 和 slab 的一致性
 */
void slub_check(void);

/* slub_print_stats - 打印统计信息
 * 显示每个 cache 的使用情况
 */
void slub_print_stats(void);

/* slub_test - 运行测试用例
 * 返回：0 表示所有测试通过，非 0 表示失败
 */
int slub_test(void);

// =============================================================================
// 内部辅助函数（不对外暴露，但在 .c 文件中实现）
// =============================================================================

// 从 PMM 分配页并初始化为 slab
struct Page *allocate_slab(struct kmem_cache *cache);

// 释放 slab 页回 PMM
void free_slab(struct kmem_cache *cache, struct Page *page);

// 根据虚拟地址获取 Page 结构
struct Page *virt_to_page(void *addr);

// 根据 Page 结构获取虚拟地址
void *page_to_virt(struct Page *page);

// 在 slab 页上初始化 freelist
void init_slab_freelist(struct kmem_cache *cache, struct Page *page);

// 根据大小选择合适的 cache
struct kmem_cache *get_cache_for_size(size_t size);

// =============================================================================
// 全局变量声明
// =============================================================================

// 预定义的缓存数组
extern struct kmem_cache *slub_caches[SLUB_SIZE_COUNT];

// SLUB 是否已初始化
extern bool slub_initialized;

#endif /* !__KERN_MM_SLUB_H__ */
