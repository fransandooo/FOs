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
#include "framebuffer.h"
#include "console.h"
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

static void boot_screen(void) {
    fb_clear(RGB(10, 10, 20));

    uint32_t cx = fb_get_width() / 2;
    uint32_t cy = fb_get_height() / 2 - 40;

    /* Title */
    fb_draw_string(cx - 12, cy, "FOs", RGB(100, 200, 255), RGB(10, 10, 20));
    fb_draw_string(cx - 92, cy + 24, "An OS built to run DOOM",
                   RGB(150, 150, 150), RGB(10, 10, 20));

    /* Decorative line */
    fb_draw_rect(cx - 120, cy + 50, 240, 1, RGB(60, 60, 100));

    /* Phase info */
    fb_draw_string(cx - 76, cy + 64, "Phase 4: Graphics",
                   RGB(80, 140, 80), RGB(10, 10, 20));

    pit_sleep_ms(1500);
}

static void demo_bouncing_box(void) {
    /* Save console state by clearing to demo background */
    uint32_t bg = RGB(10, 10, 20);
    fb_clear(bg);

    fb_draw_string(8, 8, "Bouncing box demo (500 frames)...",
                   RGB(150, 150, 150), bg);

    int x = 100, y = 100;
    int vx = 3, vy = 2;
    int w = 60, h = 60;

    for (int frame = 0; frame < 500; frame++) {
        /* Erase old */
        fb_draw_rect(x, y, w, h, bg);

        /* Update position */
        x += vx;
        y += vy;
        if (x <= 0 || x + w >= (int)fb_get_width())  { vx = -vx; x += vx; }
        if (y <= 24 || y + h >= (int)fb_get_height()) { vy = -vy; y += vy; }

        /* Draw new */
        fb_draw_rect(x, y, w, h, COLOR_CYAN);
        fb_draw_rect_outline(x, y, w, h, COLOR_WHITE);

        /* ~100 fps: wait for next timer tick */
        uint64_t next = pit_get_ticks() + 1;
        while (pit_get_ticks() < next)
            __asm__ volatile ("hlt");
    }

    /* Restore console */
    console_clear();
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
                    kprintf("Commands: help, ticks, mem, clear, demo\n");
                } else if (strcmp(line, "ticks") == 0) {
                    uint64_t t = pit_get_ticks();
                    kprintf("Ticks: %llu (uptime: %llus)\n", t, t / 100);
                } else if (strcmp(line, "mem") == 0) {
                    kprintf("Free: %llu MB (%llu pages)\n",
                            pmm_free_pages() * 4096 / (1024 * 1024),
                            pmm_free_pages());
                } else if (strcmp(line, "clear") == 0) {
                    tty_clear();
                } else if (strcmp(line, "demo") == 0) {
                    demo_bouncing_box();
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
    /* 1. Serial output first (always available) */
    serial_init();

    /* 2. VGA text mode as initial display (before framebuffer is ready) */
    tty_init();

    kprintf("========================================\n");
    kprintf("  FOs - Phase 4: Framebuffer Graphics\n");
    kprintf("========================================\n\n");

    /* 3. Memory map and management */
    e820_print();
    pmm_init();
    vmm_init();
    heap_init();

    /* 4. Interrupts */
    pic_remap();
    idt_init();
    pit_init(100);
    keyboard_init();
    pic_unmask_irq(0);
    pic_unmask_irq(1);
    __asm__ volatile ("sti");

    kprintf("\n");

    /* 5. Framebuffer graphics */
    if (fb_init()) {
        /* Switch TTY output to framebuffer console */
        console_init();
        tty_set_graphics();

        /* Show boot screen */
        boot_screen();

        /* Clear to console and re-print header */
        console_clear();
        kprintf("========================================\n");
        kprintf("  FOs - Phase 4: Framebuffer Graphics\n");
        kprintf("========================================\n\n");
        kprintf("FB: %dx%d ready\n", fb_get_width(), fb_get_height());
    } else {
        kprintf("VBE: failed, staying in text mode\n");
    }

    /* Timer test */
    uint64_t t0 = pit_get_ticks();
    pit_sleep_ms(100);
    uint64_t t1 = pit_get_ticks();
    kprintf("Timer: %llu ticks in ~100ms %s\n",
            t1 - t0, (t1 - t0 >= 8 && t1 - t0 <= 12) ? "PASS" : "CHECK");
    kprintf("Type 'help' for commands.\n\n");

    /* Interactive shell */
    shell_loop();
}
