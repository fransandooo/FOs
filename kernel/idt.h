#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint16_t offset_low;    /* Handler address bits 0-15 */
    uint16_t selector;      /* Code segment selector (0x08 = kernel CS) */
    uint8_t  ist;           /* Interrupt Stack Table offset (0 = none) */
    uint8_t  type_attr;     /* Type + DPL + present bit */
    uint16_t offset_mid;    /* Handler address bits 16-31 */
    uint32_t offset_high;   /* Handler address bits 32-63 */
    uint32_t zero;          /* Reserved */
} __attribute__((packed)) IDTEntry;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) IDTPointer;

/* Initialize IDT with exception and IRQ handlers, then load via LIDT */
void idt_init(void);

/* Set a single IDT entry */
void idt_set_entry(uint8_t vector, void *handler, uint8_t type_attr);

#endif
