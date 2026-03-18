; =============================================================================
; Kernel Entry Point
; =============================================================================
; First code that runs when the bootloader jumps to 0x100000.
; Placed in .text.boot section so the linker puts it at the very start.
;
; Responsibilities:
;   1. Clear BSS (uninitialized globals must be zero)
;   2. Set up a clean stack
;   3. Call kmain()
;   4. Halt if kmain returns (it shouldn't)
; =============================================================================

[BITS 64]

section .text.boot

global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    ; --- Clear BSS section ---
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    test rcx, rcx
    jz .bss_done
    xor al, al
    cld
    rep stosb
.bss_done:

    ; --- Set up stack ---
    ; Stack at 2MB, grows down. Kernel is at 1MB, so ~1MB of stack space.
    mov rsp, 0x200000

    ; --- Call kernel main ---
    call kmain

    ; --- If kmain returns, halt forever ---
.hang:
    cli
    hlt
    jmp .hang

; Suppress linker warning about executable stack
section .note.GNU-stack noalloc noexec nowrite progbits
