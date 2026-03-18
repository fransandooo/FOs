#include "pmm.h"
#include "memory.h"
#include "kprintf.h"
#include "string.h"

/*
 * Physical Memory Manager — Bitmap Allocator
 *
 * One bit per 4KB physical page.  0 = free, 1 = used.
 *
 * Strategy: "default deny"
 *   1. Mark everything as used (all bits = 1)
 *   2. Clear bits for E820 usable regions
 *   3. Re-mark known reserved ranges (kernel, bitmap, boot structures)
 *
 * This ensures we never accidentally allocate reserved memory.
 */

static uint8_t *bitmap = (uint8_t *)PMM_BITMAP_ADDR;
static uint64_t total_page_count;
static uint64_t free_page_count;

static inline void bm_set(uint64_t page) {
    bitmap[page / 8] |= (1 << (page % 8));
}

static inline void bm_clear(uint64_t page) {
    bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline int bm_test(uint64_t page) {
    return (bitmap[page / 8] >> (page % 8)) & 1;
}

/* Mark a range of pages as used */
static void mark_range_used(uint64_t start_page, uint64_t end_page) {
    for (uint64_t p = start_page; p < end_page; p++) {
        if (!bm_test(p)) {
            bm_set(p);
            free_page_count--;
        }
    }
}

void pmm_init(void) {
    uint16_t e820_count = *(uint16_t *)E820_COUNT_ADDR;
    E820Entry *e820_map = (E820Entry *)E820_MAP_ADDR;

    /* Find the highest USABLE physical address to determine bitmap size.
     * We ignore reserved/MMIO regions (some go up to 64GB+ for PCI space)
     * which would make the bitmap enormous and blow past our mapped memory. */
    uint64_t max_addr = 0;
    for (int i = 0; i < e820_count; i++) {
        if (e820_map[i].type != E820_USABLE) continue;
        uint64_t end = e820_map[i].base + e820_map[i].length;
        if (end > max_addr) max_addr = end;
    }

    total_page_count = max_addr / PAGE_SIZE;
    uint64_t bitmap_size = (total_page_count + 7) / 8;
    uint64_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

    /* Step 1: Mark everything as used */
    memset(bitmap, 0xFF, bitmap_size);
    free_page_count = 0;

    /* Step 2: Free pages in E820 usable regions */
    for (int i = 0; i < e820_count; i++) {
        if (e820_map[i].type != E820_USABLE) continue;

        /* Align region to page boundaries (round start up, end down) */
        uint64_t start_page = (e820_map[i].base + PAGE_SIZE - 1) / PAGE_SIZE;
        uint64_t end_page   = (e820_map[i].base + e820_map[i].length) / PAGE_SIZE;

        for (uint64_t p = start_page; p < end_page; p++) {
            bm_clear(p);
            free_page_count++;
        }
    }

    /* Step 3: Re-mark reserved regions */

    /* First 1MB — BIOS, IVT, VGA, ROM, boot structures */
    mark_range_used(0, 0x100000 / PAGE_SIZE);

    /* Kernel image (0x100000 to __kernel_end) */
    extern uint8_t __kernel_end[];
    uint64_t kernel_end_page = ((uint64_t)__kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    mark_range_used(0x100000 / PAGE_SIZE, kernel_end_page);

    /* PMM bitmap itself */
    uint64_t bm_start_page = PMM_BITMAP_ADDR / PAGE_SIZE;
    mark_range_used(bm_start_page, bm_start_page + bitmap_pages);

    kprintf("PMM: %llu MB usable (%llu pages free, %llu total)\n",
            free_page_count * PAGE_SIZE / (1024 * 1024),
            free_page_count, total_page_count);
}

void *pmm_alloc_page(void) {
    for (uint64_t p = 0; p < total_page_count; p++) {
        if (!bm_test(p)) {
            bm_set(p);
            free_page_count--;
            return (void *)(p * PAGE_SIZE);
        }
    }
    return (void *)0;   /* Out of memory */
}

void pmm_free_page(void *phys_addr) {
    uint64_t page = (uint64_t)phys_addr / PAGE_SIZE;
    if (page >= total_page_count) return;
    if (!bm_test(page)) return;     /* Already free — double-free guard */
    bm_clear(page);
    free_page_count++;
}

int pmm_reserve_page(uint64_t phys_addr) {
    uint64_t page = phys_addr / PAGE_SIZE;
    if (page >= total_page_count) return 0;
    if (bm_test(page)) return 0;    /* Already used */
    bm_set(page);
    free_page_count--;
    return 1;
}

uint64_t pmm_free_pages(void) {
    return free_page_count;
}

uint64_t pmm_total_pages(void) {
    return total_page_count;
}
