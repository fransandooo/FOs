#ifndef VMM_H
#define VMM_H

#include <stdint.h>

/*
 * Virtual Memory Manager
 *
 * Phase 2: Identity-maps all usable RAM using 2MB huge pages.
 * Provides vmm_map_page() for 4KB granularity (used by heap, future phases).
 */

/* Page table entry flags */
#define VMM_PRESENT     (1ULL << 0)
#define VMM_WRITE       (1ULL << 1)
#define VMM_USER        (1ULL << 2)
#define VMM_HUGE        (1ULL << 7)     /* 2MB page (in PD level) */
#define VMM_NO_EXEC     (1ULL << 63)

/* Initialize VMM: build proper page tables, identity-map all RAM, switch CR3 */
void vmm_init(void);

/* Map a single 4KB page: virtual → physical */
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);

/* Unmap a single 4KB page */
void vmm_unmap_page(uint64_t virt);

/* Load a new PML4 into CR3 */
void vmm_switch(uint64_t pml4_phys);

/* Read current CR3 */
uint64_t vmm_get_cr3(void);

#endif
