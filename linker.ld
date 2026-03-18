/*
 * FOs Kernel Linker Script
 *
 * Places the kernel at 0x100000 (1MB mark). This is the first usable
 * address in extended memory — everything below is either reserved by
 * the BIOS, used by the bootloader, or is legacy real-mode space.
 *
 * The .text.boot section (kernel/entry.asm) is placed first to guarantee
 * that _start is at exactly 0x100000 — this is where Stage 2 jumps to.
 *
 * Key exported symbols:
 *   __kernel_start : start of kernel image (0x100000)
 *   __kernel_end   : end of kernel image (after BSS)
 *   __bss_start    : start of BSS (must be zeroed by entry.asm)
 *   __bss_end      : end of BSS
 */

ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS {
    . = 0x100000;
    __kernel_start = .;

    .text : {
        *(.text.boot)           /* Entry point first */
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    . = ALIGN(16);
    __bss_start = .;
    .bss : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    __bss_end = .;

    . = ALIGN(4096);
    __kernel_end = .;
}
