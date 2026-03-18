; =============================================================================
; Interrupt Service Routine Stubs
; =============================================================================
; These stubs provide the assembly glue between the CPU's interrupt mechanism
; and our C interrupt handler. Each stub:
;   1. Pushes a dummy error code (if the CPU didn't push one)
;   2. Pushes the vector number
;   3. Jumps to isr_common, which saves all registers and calls C
;
; The CPU's interrupt stack frame (pushed automatically):
;   SS, RSP, RFLAGS, CS, RIP [, error_code]
;
; Our additions:
;   vector, error_code (dummy 0 if not pushed by CPU), then all GPRs
; =============================================================================

[BITS 64]

; --- Macros ---

; Exception WITHOUT error code — push dummy 0
%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0            ; Dummy error code
    push qword %1           ; Vector number
    jmp isr_common
%endmacro

; Exception WITH error code (CPU already pushed it)
%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1           ; Vector number (error code already on stack)
    jmp isr_common
%endmacro

; Hardware IRQ stub — never has error code
%macro IRQ 1
global irq%1
irq%1:
    push qword 0            ; Dummy error code
    push qword (32 + %1)    ; Vector = 32 + IRQ number
    jmp isr_common
%endmacro

; --- CPU Exceptions (vectors 0-19) ---
ISR_NOERR 0     ; #DE  Divide by Zero
ISR_NOERR 1     ; #DB  Debug
ISR_NOERR 2     ;      NMI
ISR_NOERR 3     ; #BP  Breakpoint
ISR_NOERR 4     ; #OF  Overflow
ISR_NOERR 5     ; #BR  Bound Range Exceeded
ISR_NOERR 6     ; #UD  Invalid Opcode
ISR_NOERR 7     ; #NM  Device Not Available
ISR_ERR   8     ; #DF  Double Fault
ISR_NOERR 9     ;      Coprocessor Segment Overrun (legacy)
ISR_ERR   10    ; #TS  Invalid TSS
ISR_ERR   11    ; #NP  Segment Not Present
ISR_ERR   12    ; #SS  Stack-Segment Fault
ISR_ERR   13    ; #GP  General Protection Fault
ISR_ERR   14    ; #PF  Page Fault
ISR_NOERR 15    ;      Reserved
ISR_NOERR 16    ; #MF  x87 Floating-Point Exception
ISR_ERR   17    ; #AC  Alignment Check
ISR_NOERR 18    ; #MC  Machine Check
ISR_NOERR 19    ; #XM  SIMD Floating-Point Exception

; --- Hardware IRQs (vectors 32-47 after PIC remap) ---
IRQ 0           ; PIT Timer
IRQ 1           ; PS/2 Keyboard
IRQ 2           ; Cascade (used internally by PIC, never fired)
IRQ 3           ; COM2
IRQ 4           ; COM1
IRQ 5           ; LPT2
IRQ 6           ; Floppy Disk
IRQ 7           ; LPT1 / Spurious
IRQ 8           ; CMOS RTC
IRQ 9           ; Free
IRQ 10          ; Free
IRQ 11          ; Free
IRQ 12          ; PS/2 Mouse
IRQ 13          ; FPU
IRQ 14          ; Primary ATA
IRQ 15          ; Secondary ATA / Spurious

; =============================================================================
; Common handler — saves all GPRs, calls C dispatcher, restores, iretq
; =============================================================================
isr_common:
    ; Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Call C handler: interrupt_handler(InterruptFrame *frame)
    ; Stack has 15 GPRs (120 bytes) + vector + error_code (16 bytes) = 136 bytes
    ; pushed on top of CPU's frame. 136 % 16 = 8, so RSP is misaligned.
    ; Align to 16 bytes before call (x86-64 ABI requirement).
    mov rdi, rsp            ; First arg = pointer to InterruptFrame
    sub rsp, 8              ; Align stack to 16 bytes
    extern interrupt_handler
    call interrupt_handler
    add rsp, 8              ; Remove alignment padding

    ; Restore all general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove vector number and error code from stack
    add rsp, 16

    ; Return from interrupt — restores RIP, CS, RFLAGS, RSP, SS
    iretq

; Suppress linker warning about executable stack
section .note.GNU-stack noalloc noexec nowrite progbits
