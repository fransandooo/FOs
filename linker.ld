/*
 * FOs Kernel Linker Script
 *
 * Places the kernel at 0x100000 (1MB mark). This is the first usable
 * address in extended memory — everything below is either reserved by
 * the BIOS, used by the bootloader, or is legacy real-mode space.
 *
 * The .text.boot section (kernel/entry.asm) is placed first to guarantee
 * that _start is at exactly 0x100000 — this is where Stage 2 jumps to.
 */

ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS {
    . = 0x100000;

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
}
