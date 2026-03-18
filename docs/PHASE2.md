# Phase 2: Memory Management

## Overview

Phase 2 implements the full memory management stack: physical memory detection,
physical page allocation, virtual memory mapping, and a kernel heap allocator.

## Components

### E820 Memory Map Detection (`boot/stage2.asm`)

Stage 2 was extended to detect physical memory using BIOS `int 0x15, EAX=0xE820`.
Each entry is a 24-byte struct (base, length, type). Results are stored at:

- `0x4FF8` — entry count (uint16_t)
- `0x5000` — array of E820Entry structs

QEMU with default 128MB RAM reports 7 entries, with the main usable region at
`0x100000 - 0x7FE0000` (~126 MB).

### Physical Memory Manager (`kernel/pmm.c`)

Bitmap allocator: 1 bit per 4KB page. `0 = free`, `1 = used`.

**Strategy: "default deny"**
1. Mark all pages as used (memset bitmap to 0xFF)
2. Clear bits for E820 usable regions
3. Re-mark known reserved ranges (first 1MB, kernel image, bitmap itself)

This ensures we never accidentally allocate reserved memory (BIOS data, VGA
buffer, kernel code, etc.).

**API:**
- `pmm_alloc_page()` — linear scan for first free page, returns physical address
- `pmm_free_page(addr)` — mark page as free (with double-free guard)
- `pmm_reserve_page(addr)` — mark a specific page as used
- `pmm_free_pages()` / `pmm_total_pages()` — statistics

**Layout:** Bitmap stored at `0x300000`, sized for usable RAM only (not reserved
MMIO regions that can extend to 64GB+).

### Virtual Memory Manager (`kernel/vmm.c`)

4-level x86_64 page tables (PML4 → PDPT → PD → PT).

**Identity mapping with 2MB huge pages:**
- Sets PS (Page Size) bit in PD entries to use 2MB pages
- For 128MB RAM: only 64 PD entries needed, 3 pages of overhead (PML4 + PDPT + PD)
- Compared to 67 pages if using 4KB granularity
- Maps all usable physical RAM reported by E820

**API:**
- `vmm_init()` — build page tables, switch CR3
- `vmm_map_page(virt, phys, flags)` — 4KB granularity mapping (for future use)
- `vmm_unmap_page(virt)` — remove a 4KB mapping
- `vmm_switch(pml4_phys)` — load new CR3
- `vmm_get_cr3()` — read current CR3

**Note:** `vmm_map_page()` does NOT split existing 2MB huge pages. It creates
new 4KB entries. For Phase 2, it's only safe for addresses outside the
identity-mapped range. Huge page splitting will be added in Phase 5 for
userspace.

### Kernel Heap (`kernel/heap.c`)

Free-list allocator using a doubly-linked list of block headers.

**Layout:**
- Starts at `0x400000` (identity-mapped physical memory)
- Initial size: 256 KB (64 pages reserved from PMM)
- Grows on demand by reserving contiguous pages from PMM

**BlockHeader struct (32 bytes):**
```c
typedef struct BlockHeader {
    uint64_t size;          // User data size (excluding header)
    uint8_t  is_free;
    uint8_t  _pad[7];      // Align to 16 bytes
    struct BlockHeader *next;
    struct BlockHeader *prev;
} BlockHeader;
```

**Features:**
- First-fit allocation with block splitting
- Coalescing on free (merges adjacent free blocks)
- Auto-grow: if no block fits, reserves more pages from PMM
- 8-byte alignment for all allocations

**API:**
- `kmalloc(size)` — allocate
- `kcalloc(count, size)` — allocate zeroed
- `kfree(ptr)` — free (with NULL guard)

## Memory Layout

```
0x000000 - 0x0FFFFF    Reserved (BIOS, IVT, VGA, boot structures)
0x001000 - 0x003FFF    Stage 2 page tables (temporary, before VMM)
0x004FF8              E820 entry count
0x005000 - 0x005xxx    E820 memory map entries
0x100000 - 0x101xxx    Kernel image (.text, .rodata, .data, .bss)
0x102000 - 0x104xxx    VMM page tables (PML4, PDPT, PD)
0x105000+              PMM allocations start here
0x200000              Kernel stack top (grows down)
0x300000              PMM bitmap
0x400000 - 0x43FFFF    Kernel heap (initial 256 KB, grows upward)
```

## Bugs Found and Fixed

### 1. Missing SSE disable flags (CRITICAL)

**Symptom:** Triple fault during first `kmalloc()` call, only with `-O2`.

**Root cause:** GCC `-O2` on x86_64 assumes SSE2 is available (it's part of the
x86_64 ABI baseline). It emits `movaps`, `movdqa`, etc. for struct copies and
memset operations. Our kernel doesn't enable SSE (CR0.EM=1, CR4.OSFXSR=0), so
these instructions trigger #UD (Invalid Opcode) → double fault → triple fault.

**Fix:** Added `-mno-sse -mno-sse2 -mno-mmx` to CFLAGS. This forces GCC to use
only general-purpose registers for all operations. This is standard practice for
x86_64 kernel code (Linux does the same).

### 2. Huge max_addr from E820 reserved regions

**Symptom:** Triple fault during `pmm_init()`.

**Root cause:** E820 reserved entries (PCI MMIO) reported addresses up to
`0x10000000000` (64GB). Using this as max_addr made the bitmap 2MB, which
extended past the 4MB mapped by Stage 2's temporary page tables.

**Fix:** Filter `max_addr` to only consider `E820_USABLE` entries in both
`pmm_init()` and `vmm_init()`.

### 3. split_block unsigned underflow

**Symptom:** Heap corruption leading to crashes on allocations.

**Root cause:** When `block->size < size + HEADER_SIZE`, the uint64_t subtraction
`block->size - size - HEADER_SIZE` wrapped to a huge number, creating a corrupt
free block.

**Fix:** Added pre-subtraction guard:
```c
if (block->size < size + HEADER_SIZE + MIN_SPLIT_SIZE) return;
```

### 4. Stage 2 kernel load capacity

**Change:** Increased `KERNEL_LOAD_SECTORS` from 32 to 128 (64KB max kernel
size) to accommodate the growing kernel. Both `stage2.asm` and `Makefile` have
matching constants with a size assertion at build time.

## Design Decisions

### Why bitmap allocator (not buddy/slab)?

- Simple and correct — ideal for a from-scratch kernel
- O(n) scan is fine for our page count (~32K pages)
- Buddy allocator adds complexity we don't need yet
- Slab allocator is for fixed-size objects; we use the heap for that

### Why 2MB huge pages for identity mapping?

- Minimal overhead: 3 pages vs 67 for 128MB
- Fewer TLB misses
- We don't need fine-grained permissions for kernel-only identity mapping
- 4KB granularity available via `vmm_map_page()` for specific needs

### Why free-list heap (not slab)?

- General-purpose: handles any allocation size
- Simple enough to debug in a from-scratch OS
- Coalescing prevents fragmentation
- Auto-growing means we don't need to predict heap size

### Why identity mapping (not higher-half)?

- Phase 2 keeps things simple: virtual == physical
- Higher-half kernel (common in production OSes) will be considered in Phase 5
  when we add userspace and need separate address spaces

## Test Results

```
PMM: 126 MB usable (32476 pages free, 32736 total)
VMM: identity-mapped 127 MB using 2MB huge pages
Heap: 256 KB at 0x400000

--- PMM Tests ---
Alloc 3 pages: PASS
Free/reuse: PASS

--- Heap Tests ---
Basic alloc: PASS
Write/verify: PASS
Free/reuse: PASS
Coalescing: PASS
Stress test (100 allocs, interleaved frees): PASS
```
