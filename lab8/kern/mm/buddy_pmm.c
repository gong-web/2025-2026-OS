#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>
#include <stdio.h>

/*
 * Buddy System Memory Management
 * 
 * The buddy system manages memory in blocks that are powers of two in size.
 * It maintains an array of free lists, where free_area[i] contains free blocks of size 2^i pages.
 */

// Maximum order for buddy system (2^MAX_ORDER pages)
// For example, if MAX_ORDER is 14, max block size is 16384 pages = 64MB (with 4KB pages)
#define MAX_ORDER 14

static free_area_t free_area[MAX_ORDER + 1];

#define free_list(i) (free_area[i].free_list)
#define nr_free(i) (free_area[i].nr_free)

static void
buddy_init(void) {
    for (int i = 0; i <= MAX_ORDER; i++) {
        list_init(&free_list(i));
        nr_free(i) = 0;
    }
}

// Calculate the order (power of 2) required for n pages
static int
get_order(size_t n) {
    int order = 0;
    while ((1 << order) < n) {
        order++;
    }
    return order;
}

// Check if a page block is free in the specific order list
static int
is_free(struct Page *page, int order) {
    if (PageReserved(page) || PageProperty(page)) {
        return 0;
    }
    // This is a simplified check. In a real buddy system, we need to check if the page is in the free list.
    // Here we rely on PageProperty bit which is set for the head of a free block.
    // But for buddy buddies, we need to be careful.
    // Actually, uCore's PageProperty means "Head of a free block".
    return 0; 
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    
    size_t curr_n = n;
    struct Page *curr_base = base;
    
    // We need to break down the memory range into power-of-2 blocks
    // and add them to the corresponding free lists.
    // This is a simplified initialization that assumes we can just add the whole block 
    // if it's a power of 2, or break it down.
    
    // For simplicity in this migration, let's assume we add them one by one 
    // and let the free mechanism merge them? No, that's slow.
    // Let's try to add largest possible power-of-2 blocks.
    
    uintptr_t base_addr = (uintptr_t)page2pa(base);
    
    while (curr_n > 0) {
        int order = MAX_ORDER;
        // Find the largest order that fits and is aligned
        while (order > 0) {
            size_t size = 1 << order;
            if (curr_n >= size) {
                // Check alignment if needed, but for now let's just fit size
                // In strict buddy, address must be aligned to size.
                // page_idx = (page2pa(page) - PBASE) >> PGSHIFT
                // alignment check: (page_idx % (1 << order)) == 0
                size_t page_idx = curr_base - pages; // Assuming pages is global array base
                if ((page_idx & ((1 << order) - 1)) == 0) {
                    break;
                }
            }
            order--;
        }
        
        size_t size = 1 << order;
        struct Page *p = curr_base;
        p->property = size; // Store size in property (in pages)
        SetPageProperty(p);
        
        list_add(&free_list(order), &(p->page_link));
        nr_free(order)++;
        
        curr_base += size;
        curr_n -= size;
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    int order = get_order(n);
    if (order > MAX_ORDER) {
        return NULL;
    }
    
    int cur_order = order;
    // Find the smallest block that is large enough
    while (cur_order <= MAX_ORDER) {
        if (!list_empty(&free_list(cur_order))) {
            list_entry_t *le = list_next(&free_list(cur_order));
            struct Page *page = le2page(le, page_link);
            list_del(&(page->page_link));
            nr_free(cur_order)--;
            
            // Split if necessary
            while (cur_order > order) {
                cur_order--;
                struct Page *buddy = page + (1 << cur_order);
                buddy->property = (1 << cur_order);
                SetPageProperty(buddy);
                list_add(&free_list(cur_order), &(buddy->page_link));
                nr_free(cur_order)++;
            }
            
            ClearPageProperty(page);
            return page;
        }
        cur_order++;
    }
    
    return NULL;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    int order = get_order(n);
    // Ensure n is power of 2 for buddy system consistency
    // If n is not power of 2, it means we allocated 2^k but requested n.
    // The allocator returns 2^k. When freeing, we should free 2^k.
    // But the interface passes 'n' as requested size.
    // We should probably rely on the fact that we allocated 1<<order.
    // However, standard pmm interface passes 'n'.
    // Let's assume we free 1<<order.
    
    size_t size = 1 << order;
    struct Page *p = base;
    
    for (struct Page *temp = base; temp < base + size; temp++) {
        temp->flags = 0;
        set_page_ref(temp, 0);
    }
    
    size_t page_idx = base - pages;
    
    while (order < MAX_ORDER) {
        size_t buddy_idx = page_idx ^ (1 << order);
        struct Page *buddy = pages + buddy_idx;
        
        // Check if buddy is free and has the same size
        if (!PageReserved(buddy) && PageProperty(buddy) && buddy->property == (1 << order)) {
            // Merge
            list_del(&(buddy->page_link));
            nr_free(order)--;
            ClearPageProperty(buddy);
            
            if (buddy < base) {
                base = buddy;
                page_idx = buddy_idx;
            }
            order++;
        } else {
            break;
        }
    }
    
    base->property = 1 << order;
    SetPageProperty(base);
    list_add(&free_list(order), &(base->page_link));
    nr_free(order)++;
}

static size_t
buddy_nr_free_pages(void) {
    size_t total = 0;
    for (int i = 0; i <= MAX_ORDER; i++) {
        total += nr_free(i) * (1 << i);
    }
    return total;
}

static void
buddy_check(void) {
    // Simple check
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    
    // Allocate 1 page
    p0 = alloc_pages(1);
    assert(p0 != NULL);
    
    // Allocate 2 pages
    p1 = alloc_pages(2);
    assert(p1 != NULL);
    
    // Allocate 4 pages
    p2 = alloc_pages(4);
    assert(p2 != NULL);
    
    free_pages(p0, 1);
    free_pages(p1, 2);
    free_pages(p2, 4);
    
    // Should be merged back if contiguous... 
    // This check is basic.
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
