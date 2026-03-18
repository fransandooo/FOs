#include "pic.h"
#include "io.h"

/*
 * 8259A PIC (Programmable Interrupt Controller)
 *
 * Two chips cascaded: master (IRQ0-7) and slave (IRQ8-15).
 * Slave is wired to master's IRQ2 line.
 *
 * Default BIOS mapping: IRQ0-7 → vectors 8-15, IRQ8-15 → vectors 70-77.
 * This collides with CPU exceptions (vector 8 = Double Fault, 13 = GPF).
 * We remap to: IRQ0-7 → vectors 32-47, IRQ8-15 → vectors 40-47.
 */

#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1
#define PIC_EOI     0x20

void pic_remap(void) {
    /* Save current masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* ICW1: start initialization sequence, expect ICW4 */
    outb(PIC1_CMD, 0x11);
    io_wait();
    outb(PIC2_CMD, 0x11);
    io_wait();

    /* ICW2: vector offsets */
    outb(PIC1_DATA, 0x20);  /* IRQ0 → vector 32 */
    io_wait();
    outb(PIC2_DATA, 0x28);  /* IRQ8 → vector 40 */
    io_wait();

    /* ICW3: cascade wiring */
    outb(PIC1_DATA, 0x04);  /* Master: slave on IRQ2 (bit 2) */
    io_wait();
    outb(PIC2_DATA, 0x02);  /* Slave: cascade identity = 2 */
    io_wait();

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    /* Mask all IRQs initially — we'll unmask individually as drivers init */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    (void)mask1;
    (void)mask2;
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = irq & 7;
    outb(port, inb(port) | (1 << bit));
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = irq & 7;
    outb(port, inb(port) & ~(1 << bit));

    /* If unmasking a slave IRQ, also unmask cascade line (IRQ2 on master) */
    if (irq >= 8)
        pic_unmask_irq(2);
}
