#include "isr.h"
#include "pic.h"
#include "kprintf.h"
#include "io.h"

/*
 * Central Interrupt Dispatcher
 *
 * Called from isr_common (assembly) for every interrupt. Routes to the
 * appropriate handler based on vector number:
 *   0-19  : CPU exceptions (fault → print diagnostic and halt)
 *   32-47 : Hardware IRQs (dispatch to device handler, send EOI)
 */

static const char *exception_names[] = {
    "Divide by Zero",           /*  0 */
    "Debug",                    /*  1 */
    "NMI",                      /*  2 */
    "Breakpoint",               /*  3 */
    "Overflow",                 /*  4 */
    "Bound Range Exceeded",     /*  5 */
    "Invalid Opcode",           /*  6 */
    "Device Not Available",     /*  7 */
    "Double Fault",             /*  8 */
    "Coprocessor Overrun",      /*  9 */
    "Invalid TSS",              /* 10 */
    "Segment Not Present",      /* 11 */
    "Stack-Segment Fault",      /* 12 */
    "General Protection Fault", /* 13 */
    "Page Fault",               /* 14 */
    "Reserved",                 /* 15 */
    "x87 FP Exception",        /* 16 */
    "Alignment Check",          /* 17 */
    "Machine Check",            /* 18 */
    "SIMD FP Exception",       /* 19 */
};

/* Handler function pointers for hardware IRQs (set by drivers) */
static void (*irq_handlers[16])(InterruptFrame *frame);

void isr_register_irq(uint8_t irq, void (*handler)(InterruptFrame *frame)) {
    if (irq < 16)
        irq_handlers[irq] = handler;
}

/*
 * Check if an IRQ is spurious by reading the PIC's In-Service Register (ISR).
 * IRQ7 (master) and IRQ15 (slave) can fire spuriously.
 */
static int is_spurious_irq(uint8_t irq) {
    if (irq == 7) {
        /* Read master ISR: write 0x0B to cmd port, then read it */
        outb(0x20, 0x0B);
        return !(inb(0x20) & (1 << 7));
    }
    if (irq == 15) {
        /* Read slave ISR */
        outb(0xA0, 0x0B);
        if (!(inb(0xA0) & (1 << 7))) {
            /* Spurious from slave — still need to EOI master (cascade line) */
            outb(0x20, 0x20);
            return 1;
        }
    }
    return 0;
}

void interrupt_handler(InterruptFrame *frame) {
    uint64_t vec = frame->vector;

    if (vec < 20) {
        /* CPU exception — print diagnostic and halt */
        kprintf("\n=== KERNEL PANIC ===\n");
        kprintf("Exception: %s (vector %llu)\n", exception_names[vec], vec);
        kprintf("Error code: 0x%llx\n", frame->error_code);
        kprintf("RIP:    0x%llx\n", frame->rip);
        kprintf("CS:     0x%llx\n", frame->cs);
        kprintf("RFLAGS: 0x%llx\n", frame->rflags);
        kprintf("RSP:    0x%llx\n", frame->rsp);
        if (vec == 14) {
            /* Page Fault: CR2 holds the faulting address */
            uint64_t cr2;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
            kprintf("CR2:    0x%llx\n", cr2);
        }
        kprintf("RAX: 0x%llx  RBX: 0x%llx\n", frame->rax, frame->rbx);
        kprintf("RCX: 0x%llx  RDX: 0x%llx\n", frame->rcx, frame->rdx);
        kprintf("RSI: 0x%llx  RDI: 0x%llx\n", frame->rsi, frame->rdi);
        kprintf("=== SYSTEM HALTED ===\n");
        for (;;) __asm__ volatile ("cli; hlt");

    } else if (vec >= 32 && vec < 48) {
        /* Hardware IRQ */
        uint8_t irq = vec - 32;

        /* Check for spurious IRQs on IRQ7 and IRQ15 */
        if ((irq == 7 || irq == 15) && is_spurious_irq(irq))
            return;

        /* Dispatch to registered handler */
        if (irq_handlers[irq])
            irq_handlers[irq](frame);

        pic_send_eoi(irq);
    }
    /* Vectors 20-31 and 48+ are silently ignored */
}
