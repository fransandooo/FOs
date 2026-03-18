#include "tty.h"
#include "serial.h"
#include "kprintf.h"
#include "memory.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "pic.h"
#include "idt.h"
#include "pit.h"
#include "keyboard.h"
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

static void shell_loop(void) {
    kprintf("FOs> ");

    char line[256];
    int pos = 0;

    for (;;) {
        char c = keyboard_getchar();
        if (!c) {
            __asm__ volatile ("hlt");
            continue;
        }

        if (c == '\n') {
            tty_putchar('\n');
            line[pos] = '\0';

            if (pos > 0) {
                if (strcmp(line, "help") == 0) {
                    kprintf("Commands: help, ticks, mem, clear\n");
                } else if (strcmp(line, "ticks") == 0) {
                    uint64_t t = pit_get_ticks();
                    kprintf("Ticks: %llu (uptime: %llus)\n", t, t / 100);
                } else if (strcmp(line, "mem") == 0) {
                    kprintf("Free: %llu MB (%llu pages)\n",
                            pmm_free_pages() * 4096 / (1024 * 1024),
                            pmm_free_pages());
                } else if (strcmp(line, "clear") == 0) {
                    tty_clear();
                } else {
                    kprintf("Unknown command: %s\n", line);
                }
            }

            kprintf("FOs> ");
            pos = 0;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                tty_putchar('\b');
            }
        } else if (pos < 255) {
            line[pos++] = c;
            tty_putchar(c);
        }
    }
}

void kmain(void) {
    /* 1. Output first */
    serial_init();
    tty_init();

    kprintf("========================================\n");
    kprintf("  FOs - Phase 3: Interrupts & Shell\n");
    kprintf("========================================\n\n");

    /* 2. Memory map */
    e820_print();

    /* 3. Memory management (from Phase 2) */
    pmm_init();
    vmm_init();
    heap_init();

    /* 4. Interrupts — ORDER MATTERS:
     *    Remap PIC → Load IDT → Configure devices → sti
     *    Doing sti before IDT is loaded = instant triple fault */
    pic_remap();
    idt_init();
    pit_init(100);       /* 100 Hz timer (10ms per tick) */
    keyboard_init();

    /* Unmask timer and keyboard IRQs */
    pic_unmask_irq(0);   /* IRQ0 = PIT timer */
    pic_unmask_irq(1);   /* IRQ1 = PS/2 keyboard */

    /* Enable interrupts — everything fires from here */
    __asm__ volatile ("sti");

    kprintf("\nInterrupts enabled. Timer: 100Hz, Keyboard: PS/2\n");

    /* Quick timer sanity check */
    uint64_t t0 = pit_get_ticks();
    pit_sleep_ms(100);
    uint64_t t1 = pit_get_ticks();
    kprintf("Timer test: %llu ticks in ~100ms %s\n\n",
            t1 - t0, (t1 - t0 >= 8 && t1 - t0 <= 12) ? "PASS" : "CHECK");

    /* Interactive shell */
    shell_loop();
}
