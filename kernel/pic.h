#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/* Remap PIC so IRQ0-15 map to vectors 32-47 (avoid collision with CPU exceptions) */
void pic_remap(void);

/* Send End Of Interrupt to PIC — MUST be called at end of every IRQ handler */
void pic_send_eoi(uint8_t irq);

/* Mask (disable) a specific IRQ line */
void pic_mask_irq(uint8_t irq);

/* Unmask (enable) a specific IRQ line */
void pic_unmask_irq(uint8_t irq);

#endif
