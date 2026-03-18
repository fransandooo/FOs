#include "vmm.h"
#include "pmm.h"
#include "memory.h"
#include "kprintf.h"
#include "string.h"

/*
 * Virtual Memory Manager
 *
 * Builds proper 4-level page tables to replace Stage 2's minimal setup.
 *
 * Identity mapping strategy:
 *   Uses 2MB huge pages (PD-level, PS bit set) for efficiency.
 *   For 128MB RAM, this is just 64 PD entries — 3 pages of overhead total
 *   (PML4 + PDPT + PD), versus 67 pages if using 4KB granularity.
 *
 *   The vmm_map_page() function handles 4KB granularity for specific needs
 *   (heap growth in future phases, userspace mappings, etc.).
 *
 * Important: During vmm_init(), we're still running on Stage 2's page tables
 *   (2MB huge pages mapping first 4MB). All page table pages allocated from
 *   PMM must be within this 4MB range — PMM's linear allocation ensures this.
 */

static uint64_t *kernel_pml4;

/* Mask to extract physical address from a page table entry */
#define PTE_ADDR_MASK   0x000FFFFFFFFFF000ULL

void vmm_init(void) {
    uint16_t e820_count = *(uint16_t *)E820_COUNT_ADDR;
    E820Entry *e820_map = (E820Entry *)E820_MAP_ADDR;

    /* Find highest USABLE physical address (ignore reserved MMIO regions) */
    uint64_t max_addr = 0;
    for (int i = 0; i < e820_count; i++) {
        if (e820_map[i].type != E820_USABLE) continue;
        uint64_t end = e820_map[i].base + e820_map[i].length;
        if (end > max_addr) max_addr = end;
    }

    /* Allocate PML4 */
    kernel_pml4 = (uint64_t *)pmm_alloc_page();
    memset(kernel_pml4, 0, PAGE_SIZE);

    /* Allocate PDPT (one is enough for up to 512GB) */
    uint64_t *pdpt = (uint64_t *)pmm_alloc_page();
    memset(pdpt, 0, PAGE_SIZE);
    kernel_pml4[0] = (uint64_t)pdpt | VMM_PRESENT | VMM_WRITE;

    /*
     * Create PD tables with 2MB huge page entries.
     * Each PD covers 1GB (512 entries × 2MB).
     * We need ceil(max_addr / 1GB) PD tables.
     */
    uint64_t gb = 1024ULL * 1024 * 1024;
    uint64_t pd_count = (max_addr + gb - 1) / gb;
    if (pd_count > 512) pd_count = 512;

    for (uint64_t i = 0; i < pd_count; i++) {
        uint64_t *pd = (uint64_t *)pmm_alloc_page();
        memset(pd, 0, PAGE_SIZE);
        pdpt[i] = (uint64_t)pd | VMM_PRESENT | VMM_WRITE;

        for (uint64_t j = 0; j < 512; j++) {
            uint64_t phys = (i * 512 + j) * 0x200000ULL;
            if (phys >= max_addr) break;
            pd[j] = phys | VMM_PRESENT | VMM_WRITE | VMM_HUGE;
        }
    }

    /* Switch to new page tables */
    vmm_switch((uint64_t)kernel_pml4);

    kprintf("VMM: identity-mapped %llu MB using 2MB huge pages\n",
            max_addr / (1024 * 1024));
}

/*
 * Map a single 4KB page.
 *
 * WARNING: If the target virtual address falls within a 2MB huge page,
 * this function will NOT split the huge page. It will create a conflicting
 * 4KB mapping. For Phase 2, only use this for addresses NOT covered by
 * the identity mapping (e.g., addresses above max physical RAM).
 * Huge page splitting will be added in Phase 5 when needed for userspace.
 */
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint16_t pml4_idx = (virt >> 39) & 0x1FF;
    uint16_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint16_t pd_idx   = (virt >> 21) & 0x1FF;
    uint16_t pt_idx   = (virt >> 12) & 0x1FF;

    /* Walk/create PML4 → PDPT */
    if (!(kernel_pml4[pml4_idx] & VMM_PRESENT)) {
        uint64_t *new_table = (uint64_t *)pmm_alloc_page();
        if (!new_table) return;
        memset(new_table, 0, PAGE_SIZE);
        kernel_pml4[pml4_idx] = (uint64_t)new_table | VMM_PRESENT | VMM_WRITE;
    }
    uint64_t *pdpt = (uint64_t *)(kernel_pml4[pml4_idx] & PTE_ADDR_MASK);

    /* Walk/create PDPT → PD */
    if (!(pdpt[pdpt_idx] & VMM_PRESENT)) {
        uint64_t *new_table = (uint64_t *)pmm_alloc_page();
        if (!new_table) return;
        memset(new_table, 0, PAGE_SIZE);
        pdpt[pdpt_idx] = (uint64_t)new_table | VMM_PRESENT | VMM_WRITE;
    }
    uint64_t *pd = (uint64_t *)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    /* Walk/create PD → PT */
    if (!(pd[pd_idx] & VMM_PRESENT)) {
        uint64_t *new_table = (uint64_t *)pmm_alloc_page();
        if (!new_table) return;
        memset(new_table, 0, PAGE_SIZE);
        pd[pd_idx] = (uint64_t)new_table | VMM_PRESENT | VMM_WRITE;
    }
    uint64_t *pt = (uint64_t *)(pd[pd_idx] & PTE_ADDR_MASK);

    /* Set the page table entry */
    pt[pt_idx] = phys | flags | VMM_PRESENT;

    /* Flush TLB for this virtual address */
    __asm__ volatile ("invlpg (%0)" :: "r"(virt) : "memory");
}

void vmm_unmap_page(uint64_t virt) {
    uint16_t pml4_idx = (virt >> 39) & 0x1FF;
    uint16_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint16_t pd_idx   = (virt >> 21) & 0x1FF;
    uint16_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(kernel_pml4[pml4_idx] & VMM_PRESENT)) return;
    uint64_t *pdpt = (uint64_t *)(kernel_pml4[pml4_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & VMM_PRESENT)) return;
    uint64_t *pd = (uint64_t *)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    if (!(pd[pd_idx] & VMM_PRESENT)) return;
    uint64_t *pt = (uint64_t *)(pd[pd_idx] & PTE_ADDR_MASK);

    pt[pt_idx] = 0;
    __asm__ volatile ("invlpg (%0)" :: "r"(virt) : "memory");
}

void vmm_switch(uint64_t pml4_phys) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

uint64_t vmm_get_cr3(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}
