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
 *     // SLUB 专用字段（使用 union 节省空间）
 *     struct kmem_cache *slab_cache;  // 所属的 cache
 *     void *freelist;                 // 页内的空闲对象链表
 *     void *s_mem;                    // 页内第一个对象的地址
 *     unsigned int inuse;             // 已分配对象计数
 *     unsigned int objects;           // 总对象数
 * };
 */

/* ============================================
 * 函数原型（学号：2312325）
 * ============================================ */

// 初始化
void slub_init(void);

// 缓存池管理
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align, unsigned long flags, void (*ctor)(void *));
void kmem_cache_destroy(struct kmem_cache *cache);

// 对象分配与释放
void *kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(struct kmem_cache *cache, void *obj);

// 通用分配接口（类似 malloc/free）
void *kmalloc(size_t size);
void kfree(void *obj);

// 辅助函数
struct Page *virt_to_page(void *addr);
void *page_to_virt(struct Page *page);
struct Page *allocate_slab(struct kmem_cache *cache);
void free_slab(struct kmem_cache *cache, struct Page *page);

#endif /* ! __KERN_MM_SLUB_H__ */
