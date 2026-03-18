#include "serial.h"
#include "io.h"

/*
 * Serial Port Driver (COM1)
 *
 * Invaluable for debugging — output appears in your terminal when you run:
 *   qemu-system-x86_64 -drive format=raw,file=fos.img -serial stdio
 *
 * This works even when VGA is broken, making it the first output channel
 * you should set up and the last one you should abandon.
 */

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);      /* Disable all interrupts */
    outb(COM1 + 3, 0x80);      /* Enable DLAB (set baud rate divisor) */
    outb(COM1 + 0, 0x03);      /* Divisor lo byte: 38400 baud */
    outb(COM1 + 1, 0x00);      /* Divisor hi byte */
    outb(COM1 + 3, 0x03);      /* 8 bits, no parity, 1 stop bit (8N1) */
    outb(COM1 + 2, 0xC7);      /* Enable FIFO, clear them, 14-byte threshold */
    outb(COM1 + 4, 0x0B);      /* IRQs enabled, RTS/DSR set */
}

void serial_putchar(char c) {
    /* Wait until transmit holding register is empty */
    while ((inb(COM1 + 5) & 0x20) == 0)
        ;
    outb(COM1, c);
}

void serial_print(const char *str) {
    while (*str)
        serial_putchar(*str++);
}
