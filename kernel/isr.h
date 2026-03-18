#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/*
 * Interrupt stack frame — matches the push order in isr_stubs.asm.
 *
 * When an interrupt fires, the CPU pushes SS, RSP, RFLAGS, CS, RIP
 * (and error code for some exceptions). Our stub then pushes vector,
 * error code (dummy 0 if none), and all general-purpose registers.
 */
typedef struct {
    /* Saved by our stub (pushed in reverse order, so first field = last push) */
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    /* Pushed by stub */
    uint64_t vector;
    uint64_t error_code;
    /* Pushed by CPU */
    uint64_t rip, cs, rflags, rsp, ss;
} InterruptFrame;

/* Central C interrupt dispatcher — called from isr_common in assembly */
void interrupt_handler(InterruptFrame *frame);

/* Register a handler for a hardware IRQ (0-15) */
void isr_register_irq(uint8_t irq, void (*handler)(InterruptFrame *frame));

#endif
