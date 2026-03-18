#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

/*
 * E820 Memory Map
 *
 * Populated by Stage 2 (real mode) via BIOS int 0x15, EAX=0xE820.
 * Stored at fixed physical addresses (identity-mapped).
 */
#define E820_MAP_ADDR    0x5000
#define E820_COUNT_ADDR  0x4FF8

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;          /* 1=usable, 2=reserved, 3=ACPI reclaimable */
    uint32_t acpi_attrs;    /* ACPI 3.0 extended attributes */
} __attribute__((packed)) E820Entry;

/* E820 type values */
#define E820_USABLE         1
#define E820_RESERVED       2
#define E820_ACPI_RECLAIM   3
#define E820_ACPI_NVS       4
#define E820_BAD_MEMORY     5

/*
 * Physical memory layout (fixed addresses used by kernel subsystems)
 *
 * 0x100000              : Kernel start (1MB mark)
 * __kernel_end          : Kernel end (from linker script)
 * ↓ from 0x200000       : Stack (grows down, ~1MB space)
 * 0x300000              : PMM bitmap
 * 0x400000              : Kernel heap start
 *
 * Note: Stack at 0x200000 grows DOWN toward kernel.
 *       Bitmap at 0x300000 grows UP.
 *       These do not overlap (1MB gap).
 */
#define KERNEL_PHYS_BASE    0x100000
#define STACK_TOP           0x200000
#define PMM_BITMAP_ADDR     0x300000
#define HEAP_START          0x400000
#define HEAP_INITIAL_SIZE   (64 * 4096)     /* 256KB initial heap */

#define PAGE_SIZE           4096

#endif
