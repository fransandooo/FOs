#ifndef PIT_H
#define PIT_H

#include <stdint.h>

/* Initialize PIT channel 0 to fire IRQ0 at the given frequency (Hz) */
void pit_init(uint32_t hz);

/* Get current tick count (incremented by timer IRQ handler) */
uint64_t pit_get_ticks(void);

/* Busy-wait for approximately ms milliseconds (requires interrupts enabled) */
void pit_sleep_ms(uint64_t ms);

#endif
