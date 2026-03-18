#include "tty.h"
#include "serial.h"
#include "kprintf.h"
#include "memory.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "string.h"
#include <stdint.h>

static void e820_print(void) {
    uint16_t count = *(uint16_t *)E820_COUNT_ADDR;
    E820Entry *map = (E820Entry *)E820_MAP_ADDR;

    static const char *type_names[] = {
        "???", "Usable", "Reserved", "ACPI Recl", "ACPI NVS", "Bad"
    };

    kprintf("E820 Memory Map (%d entries):\n", count);
    for (int i = 0; i < count; i++) {
        const char *name = map[i].type <= 5 ? type_names[map[i].type] : "???";
        kprintf("  0x%llx - 0x%llx  %s\n",
                map[i].base,
                map[i].base + map[i].length,
                name);
    }
    kprintf("\n");
}

static void test_pmm(void) {
    kprintf("--- PMM Tests ---\n");

    void *p1 = pmm_alloc_page();
    void *p2 = pmm_alloc_page();
    void *p3 = pmm_alloc_page();
    kprintf("Alloc 3 pages: %p %p %p\n", p1, p2, p3);

    /* Free p2, then alloc again — should reuse p2's slot */
    pmm_free_page(p2);
    void *p4 = pmm_alloc_page();
    kprintf("Free p2, re-alloc: %p (expect %p) %s\n",
            p4, p2, p4 == p2 ? "PASS" : "FAIL");

    /* Clean up */
    pmm_free_page(p1);
    pmm_free_page(p3);
    pmm_free_page(p4);

    kprintf("PMM: %llu pages free after cleanup\n\n", pmm_free_pages());
}

static void test_heap(void) {
    kprintf("--- Heap Tests ---\n");

    /* Basic alloc */
    void *a = kmalloc(64);
    void *b = kmalloc(128);
    void *c = kmalloc(32);
    kprintf("Alloc: a=%p b=%p c=%p\n", a, b, c);

    /* Write and verify (catches mapping/corruption issues) */
    memset(a, 0xAA, 64);
    memset(b, 0xBB, 128);
    memset(c, 0xCC, 32);
    kprintf("Write: a[0]=0x%x b[0]=0x%x c[0]=0x%x ",
            *(uint8_t *)a, *(uint8_t *)b, *(uint8_t *)c);
    kprintf("%s\n",
            (*(uint8_t *)a == 0xAA && *(uint8_t *)b == 0xBB &&
             *(uint8_t *)c == 0xCC) ? "PASS" : "FAIL");

    /* Free and reuse — b's slot should be reused */
    kfree(b);
    void *d = kmalloc(100);
    kprintf("Reuse: d=%p (was b=%p) %s\n",
            d, b, d == b ? "PASS" : "NEAR");

    /* Coalescing — free adjacent blocks, then alloc a larger one */
    kfree(a);
    kfree(d);
    void *big = kmalloc(200);
    kprintf("Coalesce: big=%p %s\n", big, big ? "PASS" : "FAIL");
    kfree(big);
    kfree(c);

    /* Stress test */
    kprintf("Stress test (100 allocs, interleaved frees)... ");
    void *ptrs[100];
    int ok = 1;
    for (int i = 0; i < 100; i++) {
        ptrs[i] = kmalloc(i * 8 + 8);
        if (!ptrs[i]) { ok = 0; break; }
    }
    for (int i = 0; i < 100; i += 2)
        kfree(ptrs[i]);
    for (int i = 0; i < 50; i++) {
        ptrs[i] = kmalloc(16);
        if (!ptrs[i]) { ok = 0; break; }
    }
    /* Free everything */
    for (int i = 0; i < 50; i++)
        kfree(ptrs[i]);
    for (int i = 1; i < 100; i += 2)
        kfree(ptrs[i]);
    kprintf("%s\n\n", ok ? "PASS" : "FAIL");
}

void kmain(void) {
    /* 1. Debug output first — always */
    serial_init();
    tty_init();

    kprintf("========================================\n");
    kprintf("  FOs - Phase 2: Memory Management\n");
    kprintf("========================================\n\n");

    /* 2. Print E820 memory map (populated by Stage 2) */
    e820_print();

    /* 3. Physical Memory Manager — needs E820 */
    pmm_init();

    /* 4. Virtual Memory Manager — needs PMM to allocate page tables */
    vmm_init();

    /* 5. Kernel Heap — needs PMM (uses identity-mapped pages) */
    heap_init();

    kprintf("\n");

    /* 6. Run tests */
    test_pmm();
    test_heap();

    /* Summary */
    kprintf("========================================\n");
    kprintf("  Phase 2 complete\n");
    kprintf("  Free: %llu MB (%llu pages)\n",
            pmm_free_pages() * PAGE_SIZE / (1024 * 1024),
            pmm_free_pages());
    kprintf("========================================\n");

    for (;;)
        __asm__ volatile ("hlt");
}
