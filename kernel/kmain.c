#include "tty.h"
#include "serial.h"

/*
 * Kernel entry point.
 *
 * Called by kernel/entry.asm after BSS is cleared and stack is set up.
 * For Phase 1, we just prove the entire boot chain works: BIOS → Stage 1 →
 * Stage 2 (real → protected → long mode) → this C function.
 */

void kmain(void) {
    serial_init();
    serial_print("[FOs] Kernel started (serial)\n");

    tty_init();
    tty_print("========================================\n");
    tty_print("  FOs - An OS built to run DOOM\n");
    tty_print("========================================\n");
    tty_print("\n");
    tty_print("Hello from FOs!\n");
    tty_print("Phase 1 complete: booted to 64-bit long mode.\n");
    tty_print("\n");
    tty_print("Boot chain: BIOS -> Stage1 -> Stage2 -> Kernel\n");
    tty_print("CPU mode:   Real -> Protected -> Long (64-bit)\n");
    tty_print("\n");

    /* Halt forever — nothing else to do yet */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
