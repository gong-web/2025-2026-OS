#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* This file contains the definitions for memory management in our OS. */

/* *
 * Virtual memory map:                                          Permissions
 *                                                              kernel/user
 *
 *     4G ------------------> +---------------------------------+
 *                            |                                 |
 *                            |         Empty Memory (*)        |
 *                            |                                 |
 *                            +---------------------------------+ 0xFB000000
 *                            |   Cur. Page Table (Kern, RW)    | RW/-- PTSIZE
 *     VPT -----------------> +---------------------------------+ 0xFAC00000
 *                            |        Invalid Memory (*)       | --/--
 *     KERNTOP -------------> +---------------------------------+ 0xF8000000
 *                            |                                 |
 *                            |    Remapped Physical Memory     | RW/-- KMEMSIZE
 *                            |                                 |
 *     KERNBASE ------------> +---------------------------------+ 0xC0000000
 *                            |        Invalid Memory (*)       | --/--
 *     USERTOP -------------> +---------------------------------+ 0xB0000000
 *                            |           User stack            |
 *                            +---------------------------------+
 *                            |                                 |
 *                            :                                 :
 *                            |         ~~~~~~~~~~~~~~~~        |
 *                            :                                 :
 *                            |                                 |
 *                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                            |       User Program & Heap       |
 *     UTEXT ---------------> +---------------------------------+ 0x00800000
 *                            |        Invalid Memory (*)       | --/--
 *                            |  - - - - - - - - - - - - - - -  |
 *                            |    User STAB Data (optional)    |
 *     USERBASE, USTAB------> +---------------------------------+ 0x00200000
 *                            |        Invalid Memory (*)       | --/--
 *     0 -------------------> +---------------------------------+ 0x00000000
 * (*) Note: The kernel ensures that "Invalid Memory" is *never* mapped.
 *     "Empty Memory" is normally unmapped, but user programs may map pages
 *     there if desired.
 *
 * 虚拟内存映射图说明 (基于 RISC-V 64位 Sv39 模式的适配):
 * 注意：上面的图示是基于32位系统的经典布局，但下面的宏定义显示这是一个64位系统。
 *
 * 关键地址定义:
 * - KERNBASE: 内核虚拟地址空间的起始位置 (偏移量映射)
 * - USERTOP:  用户空间可访问的最高虚拟地址
 * - USTACKTOP: 用户栈顶地址
 * - USERBASE: 用户空间起始地址
 * - UTEXT:    用户程序代码段起始地址
 * */

/* All physical memory mapped at this address */
// 所有物理内存映射到的虚拟地址基址 (内核空间起始)
#define KERNBASE 0xFFFFFFFFC0200000
#define KMEMSIZE 0x7E00000 // the maximum amount of physical memory (最大物理内存大小)
#define KERNTOP (KERNBASE + KMEMSIZE) // 内核空间顶部

#define PHYSICAL_MEMORY_OFFSET 0xFFFFFFFF40000000 // 物理内存偏移量

/* *
 * Virtual page table. Entry PDX[VPT] in the PD (Page Directory) contains
 * a pointer to the page directory itself, thereby turning the PD into a page
 * table, which maps all the PTEs (Page Table Entry) containing the page mappings
 * for the entire virtual address space into that 4 Meg region starting at VPT.
 * */

#define KSTACKPAGE 2                     // # of pages in kernel stack (内核栈页数)
#define KSTACKSIZE (KSTACKPAGE * PGSIZE) // sizeof kernel stack (内核栈大小)

#define USERTOP 0x80000000               // 用户空间最高地址
#define USTACKTOP USERTOP                // 用户栈顶 = 用户空间最高地址
#define USTACKPAGE 256                   // # of pages in user stack (用户栈页数)
#define USTACKSIZE (USTACKPAGE * PGSIZE) // sizeof user stack (用户栈大小)

#define USERBASE 0x00200000              // 用户空间起始地址
#define UTEXT 0x00800000                 // where user programs generally begin (用户程序代码段起始)
#define USTAB USERBASE                   // the location of the user STABS data structure

// 检查地址是否在用户空间范围内
#define USER_ACCESS(start, end) \
    (USERBASE <= (start) && (start) < (end) && (end) <= USERTOP)

// 检查地址是否在内核空间范围内
#define KERN_ACCESS(start, end) \
    (KERNBASE <= (start) && (start) < (end) && (end) <= KERNTOP)

#ifndef __ASSEMBLER__

#include <defs.h>
#include <atomic.h>
#include <list.h>

typedef uintptr_t pte_t; // 页表项类型
typedef uintptr_t pde_t; // 页目录项类型
typedef pte_t swap_entry_t; // the pte can also be a swap entry (交换条目类型)

/* *
 * struct Page - Page descriptor structures. Each Page describes one
 * physical page. In kern/mm/pmm.h, you can find lots of useful functions
 * that convert Page to other data types, such as physical address.
 *
 * Page 结构体 - 物理页描述符。
 * 每一个 Page 结构体对应管理一个物理页。
 * */
struct Page
{
    int ref;                    // page frame's reference counter (页面引用计数)
                                // 当 ref > 0 时，表示该页被占用；为 0 时表示空闲。
    uint64_t flags;             // array of flags that describe the status of the page frame (状态标志位)
    unsigned int property;      // the num of free block, used in first fit pm manager (空闲块数量)
                                // 仅在是空闲块的头页(head page)时有效，表示连续空闲页的数量。
    list_entry_t page_link;     // free list link (空闲链表节点)
                                // 用于将空闲页链接到 free_list 中。
    list_entry_t pra_page_link; // used for pra (page replace algorithm) (页面置换算法链表节点)
    uintptr_t pra_vaddr;        // used for pra (page replace algorithm) (该页对应的虚拟地址)
                                // 用于页面置换算法记录该物理页被映射到的虚拟地址。
};

/* Flags describing the status of a page frame */
// 描述页面状态的标志位
#define PG_reserved 0 // if this bit=1: the Page is reserved for kernel, cannot be used in alloc/free_pages; otherwise, this bit=0
                      // 保留位：如果置1，表示该页被内核保留，不能用于分配。
#define PG_property 1 // if this bit=1: the Page is the head page of a free memory block(contains some continuous_addrress pages), and can be used in alloc_pages; if this bit=0: if the Page is the the head page of a free memory block, then this Page and the memory block is alloced. Or this Page isn't the head page.
                      // 属性位：如果置1，表示该页是一个空闲块的头页(head page)，property字段有效。

// 设置/清除/测试保留位
#define SetPageReserved(page) set_bit(PG_reserved, &((page)->flags))
#define ClearPageReserved(page) clear_bit(PG_reserved, &((page)->flags))
#define PageReserved(page) test_bit(PG_reserved, &((page)->flags))

// 设置/清除/测试属性位
#define SetPageProperty(page) set_bit(PG_property, &((page)->flags))
#define ClearPageProperty(page) clear_bit(PG_property, &((page)->flags))
#define PageProperty(page) test_bit(PG_property, &((page)->flags))

// convert list entry to page (将链表节点转换为 Page 结构体指针)
#define le2page(le, member) \
    to_struct((le), struct Page, member)

/* free_area_t - maintains a doubly linked list to record free (unused) pages */
/* free_area_t - 维护一个双向链表来记录空闲页 */
typedef struct
{
    list_entry_t free_list; // the list header (空闲链表头)
    unsigned int nr_free;   // # of free pages in this free list (空闲页总数)
} free_area_t;

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */
