#include "idt.h"
#include "string.h"

#define IDT_SIZE   256
#define KERNEL_CS  0x08    /* Must match 64-bit GDT code segment from stage2 */

/* 0x8E = present, DPL=0, 64-bit interrupt gate (clears IF on entry) */
#define IDT_INTERRUPT_GATE 0x8E

static IDTEntry idt[IDT_SIZE];
static IDTPointer idt_ptr;

void idt_set_entry(uint8_t vector, void *handler, uint8_t type_attr) {
    uint64_t addr = (uint64_t)handler;
    idt[vector].offset_low  = addr & 0xFFFF;
    idt[vector].selector    = KERNEL_CS;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].zero        = 0;
}

void idt_init(void) {
    memset(idt, 0, sizeof(idt));

    /* CPU Exceptions (vectors 0-19) */
    extern void isr0(void);   extern void isr1(void);
    extern void isr2(void);   extern void isr3(void);
    extern void isr4(void);   extern void isr5(void);
    extern void isr6(void);   extern void isr7(void);
    extern void isr8(void);   extern void isr9(void);
    extern void isr10(void);  extern void isr11(void);
    extern void isr12(void);  extern void isr13(void);
    extern void isr14(void);  extern void isr15(void);
    extern void isr16(void);  extern void isr17(void);
    extern void isr18(void);  extern void isr19(void);

    idt_set_entry(0,  isr0,  IDT_INTERRUPT_GATE);
    idt_set_entry(1,  isr1,  IDT_INTERRUPT_GATE);
    idt_set_entry(2,  isr2,  IDT_INTERRUPT_GATE);
    idt_set_entry(3,  isr3,  IDT_INTERRUPT_GATE);
    idt_set_entry(4,  isr4,  IDT_INTERRUPT_GATE);
    idt_set_entry(5,  isr5,  IDT_INTERRUPT_GATE);
    idt_set_entry(6,  isr6,  IDT_INTERRUPT_GATE);
    idt_set_entry(7,  isr7,  IDT_INTERRUPT_GATE);
    idt_set_entry(8,  isr8,  IDT_INTERRUPT_GATE);
    idt_set_entry(9,  isr9,  IDT_INTERRUPT_GATE);
    idt_set_entry(10, isr10, IDT_INTERRUPT_GATE);
    idt_set_entry(11, isr11, IDT_INTERRUPT_GATE);
    idt_set_entry(12, isr12, IDT_INTERRUPT_GATE);
    idt_set_entry(13, isr13, IDT_INTERRUPT_GATE);
    idt_set_entry(14, isr14, IDT_INTERRUPT_GATE);
    idt_set_entry(15, isr15, IDT_INTERRUPT_GATE);
    idt_set_entry(16, isr16, IDT_INTERRUPT_GATE);
    idt_set_entry(17, isr17, IDT_INTERRUPT_GATE);
    idt_set_entry(18, isr18, IDT_INTERRUPT_GATE);
    idt_set_entry(19, isr19, IDT_INTERRUPT_GATE);

    /* Hardware IRQs (vectors 32-47, after PIC remap) */
    extern void irq0(void);   extern void irq1(void);
    extern void irq2(void);   extern void irq3(void);
    extern void irq4(void);   extern void irq5(void);
    extern void irq6(void);   extern void irq7(void);
    extern void irq8(void);   extern void irq9(void);
    extern void irq10(void);  extern void irq11(void);
    extern void irq12(void);  extern void irq13(void);
    extern void irq14(void);  extern void irq15(void);

    idt_set_entry(32, irq0,  IDT_INTERRUPT_GATE);
    idt_set_entry(33, irq1,  IDT_INTERRUPT_GATE);
    idt_set_entry(34, irq2,  IDT_INTERRUPT_GATE);
    idt_set_entry(35, irq3,  IDT_INTERRUPT_GATE);
    idt_set_entry(36, irq4,  IDT_INTERRUPT_GATE);
    idt_set_entry(37, irq5,  IDT_INTERRUPT_GATE);
    idt_set_entry(38, irq6,  IDT_INTERRUPT_GATE);
    idt_set_entry(39, irq7,  IDT_INTERRUPT_GATE);
    idt_set_entry(40, irq8,  IDT_INTERRUPT_GATE);
    idt_set_entry(41, irq9,  IDT_INTERRUPT_GATE);
    idt_set_entry(42, irq10, IDT_INTERRUPT_GATE);
    idt_set_entry(43, irq11, IDT_INTERRUPT_GATE);
    idt_set_entry(44, irq12, IDT_INTERRUPT_GATE);
    idt_set_entry(45, irq13, IDT_INTERRUPT_GATE);
    idt_set_entry(46, irq14, IDT_INTERRUPT_GATE);
    idt_set_entry(47, irq15, IDT_INTERRUPT_GATE);

    /* Load IDT */
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint64_t)idt;
    __asm__ volatile ("lidt %0" :: "m"(idt_ptr));
}
