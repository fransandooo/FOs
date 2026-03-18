#ifndef PMM_H
#define PMM_H

#include <stdint.h>

/* Initialize PMM from E820 memory map */
void pmm_init(void);

/* Allocate a single 4KB physical page. Returns physical address, or 0 on OOM. */
void *pmm_alloc_page(void);

/* Free a physical page */
void pmm_free_page(void *phys_addr);

/* Reserve a specific physical page (mark as used). Returns 1 on success. */
int pmm_reserve_page(uint64_t phys_addr);

/* Get count of free pages */
uint64_t pmm_free_pages(void);

/* Get total tracked pages */
uint64_t pmm_total_pages(void);

#endif
