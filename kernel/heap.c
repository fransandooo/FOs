#include "heap.h"
#include "memory.h"
#include "pmm.h"
#include "kprintf.h"
#include "string.h"

/*
 * Kernel Heap — Free-List Allocator
 *
 * Lives in identity-mapped physical memory starting at HEAP_START (0x400000).
 * Pages are reserved contiguously from PMM. In Phase 2 there's no
 * fragmentation, so contiguous physical pages are always available.
 *
 * Each allocation is preceded by a BlockHeader that tracks size, free status,
 * and links to adjacent blocks (doubly-linked list). On free, adjacent free
 * blocks are coalesced to prevent fragmentation.
 *
 * This is a first-fit allocator. Simple, correct, good enough for a kernel.
 */

typedef struct BlockHeader {
    uint64_t size;              /* Size of user data (excluding header) */
    uint8_t  is_free;
    uint8_t  _pad[7];          /* Align to 16 bytes */
    struct BlockHeader *next;
    struct BlockHeader *prev;
} BlockHeader;

#define HEADER_SIZE     sizeof(BlockHeader)  /* 32 bytes */
#define MIN_SPLIT_SIZE  32                   /* Don't split if remainder < this */

static BlockHeader *heap_head;
static uint64_t heap_end;

/* Split a block if there's enough room for a new free block after the allocation */
static void split_block(BlockHeader *block, uint64_t size) {
    /* Guard against unsigned underflow: if block isn't big enough to hold
     * the allocation PLUS a new header PLUS minimum useful free space,
     * just use the whole block without splitting. */
    if (block->size < size + HEADER_SIZE + MIN_SPLIT_SIZE) return;
    uint64_t remaining = block->size - size - HEADER_SIZE;

    BlockHeader *new_block = (BlockHeader *)((uint8_t *)block + HEADER_SIZE + size);
    new_block->size    = remaining;
    new_block->is_free = 1;
    new_block->next    = block->next;
    new_block->prev    = block;

    if (block->next) block->next->prev = new_block;
    block->next = new_block;
    block->size = size;
}

/* Merge a free block with its next neighbor if also free */
static void coalesce(BlockHeader *block) {
    if (block->next && block->next->is_free) {
        block->size += HEADER_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }
}

/* Grow the heap by reserving more contiguous physical pages */
static int heap_grow(uint64_t min_bytes) {
    uint64_t needed = min_bytes + HEADER_SIZE;
    uint64_t pages = (needed + PAGE_SIZE - 1) / PAGE_SIZE;

    /* Reserve contiguous pages from PMM */
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t page_addr = heap_end + i * PAGE_SIZE;
        if (!pmm_reserve_page(page_addr)) {
            /* Page not free — can't grow contiguously */
            kprintf("heap: can't reserve page at 0x%llx\n", page_addr);
            return 0;
        }
    }

    uint64_t grow_size = pages * PAGE_SIZE;

    /* Find the last block */
    BlockHeader *last = heap_head;
    while (last->next) last = last->next;

    if (last->is_free) {
        /* Extend the last free block */
        last->size += grow_size;
    } else {
        /* Create a new free block in the expanded space */
        BlockHeader *new_block = (BlockHeader *)((uint8_t *)last + HEADER_SIZE + last->size);
        new_block->size    = grow_size - HEADER_SIZE;
        new_block->is_free = 1;
        new_block->next    = NULL;
        new_block->prev    = last;
        last->next = new_block;
    }

    heap_end += grow_size;
    return 1;
}

void heap_init(void) {
    /* Reserve initial heap pages from PMM */
    uint64_t init_pages = HEAP_INITIAL_SIZE / PAGE_SIZE;
    for (uint64_t i = 0; i < init_pages; i++) {
        uint64_t page_addr = HEAP_START + i * PAGE_SIZE;
        pmm_reserve_page(page_addr);
    }

    /* Initialize the free list with a single large free block */
    heap_head = (BlockHeader *)HEAP_START;
    heap_head->size    = HEAP_INITIAL_SIZE - HEADER_SIZE;
    heap_head->is_free = 1;
    heap_head->next    = NULL;
    heap_head->prev    = NULL;
    heap_end = HEAP_START + HEAP_INITIAL_SIZE;

    kprintf("Heap: %llu KB at 0x%llx\n",
            (uint64_t)HEAP_INITIAL_SIZE / 1024,
            (uint64_t)HEAP_START);
}

void *kmalloc(uint64_t size) {
    if (!size) return NULL;

    /* Align to 8 bytes */
    size = (size + 7) & ~7ULL;

    /* First-fit search */
    BlockHeader *cur = heap_head;
    while (cur) {
        if (cur->is_free && cur->size >= size) {
            split_block(cur, size);
            cur->is_free = 0;
            return (void *)((uint8_t *)cur + HEADER_SIZE);
        }
        cur = cur->next;
    }

    /* No fit — grow the heap and retry */
    if (heap_grow(size)) {
        return kmalloc(size);
    }

    kprintf("kmalloc: out of memory (requested %llu bytes)\n", size);
    return NULL;
}

void *kcalloc(uint64_t count, uint64_t size) {
    uint64_t total = count * size;
    void *ptr = kmalloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void kfree(void *ptr) {
    if (!ptr) return;

    BlockHeader *block = (BlockHeader *)((uint8_t *)ptr - HEADER_SIZE);
    block->is_free = 1;

    /* Coalesce with next neighbor */
    coalesce(block);

    /* Coalesce with previous neighbor */
    if (block->prev && block->prev->is_free) {
        coalesce(block->prev);
    }
}
