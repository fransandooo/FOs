#include "pit.h"
#include "isr.h"
#include "io.h"

/*
 * Intel 8253/8254 Programmable Interval Timer
 *
 * Channel 0 generates IRQ0 at a configurable frequency.
 * Base oscillator: 1,193,182 Hz. We set a divisor to get our target rate.
 *
 * We use 100 Hz (10ms per tick) — clean number, low overhead, good enough
 * for timekeeping. Preemptive scheduling can use the same tick or bump the
 * rate later.
 */

#define PIT_CMD      0x43
#define PIT_CH0      0x40
#define PIT_BASE_HZ  1193182

static volatile uint64_t ticks = 0;
static uint32_t tick_hz = 100;

static void timer_irq_handler(InterruptFrame *frame) {
    (void)frame;
    ticks++;
}

void pit_init(uint32_t hz) {
    tick_hz = hz;
    uint32_t divisor = PIT_BASE_HZ / hz;

    /* Clamp divisor to 16-bit range */
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor < 1)      divisor = 1;

    /* Channel 0, lo/hi byte access, mode 3 (square wave generator) */
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, divisor & 0xFF);
    outb(PIT_CH0, (divisor >> 8) & 0xFF);

    /* Register our handler for IRQ0 */
    isr_register_irq(0, timer_irq_handler);
}

uint64_t pit_get_ticks(void) {
    return ticks;
}

void pit_sleep_ms(uint64_t ms) {
    uint64_t target_ticks = (ms * tick_hz + 999) / 1000;
    uint64_t end = ticks + target_ticks;
    while (ticks < end)
        __asm__ volatile ("hlt");
}
