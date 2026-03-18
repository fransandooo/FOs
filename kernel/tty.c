#include "tty.h"
#include "serial.h"
#include "io.h"
#include <stdint.h>

/*
 * VGA Text Mode Driver
 *
 * The VGA text buffer is a memory-mapped I/O region at physical address
 * 0xB8000. It's an 80x25 grid of 16-bit entries:
 *
 *   Bits 0-7  : ASCII character code
 *   Bits 8-11 : Foreground color
 *   Bits 12-14: Background color
 *   Bit  15   : Blink (or bright background, depending on VGA config)
 *
 * Color 0x0F = white on black (our default).
 */

#define VGA_ADDRESS  0xB8000
#define VGA_WIDTH    80
#define VGA_HEIGHT   25
#define VGA_COLOR    0x0F        /* White text on black background */

static volatile uint16_t *vga = (volatile uint16_t *)VGA_ADDRESS;
static int col = 0;
static int row = 0;

static uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/* Update the hardware cursor position via VGA CRT controller */
static void update_cursor(void) {
    uint16_t pos = row * VGA_WIDTH + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* Scroll the screen up by one line */
static void scroll(void) {
    /* Move all rows up by one */
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        vga[i] = vga[i + VGA_WIDTH];
    }
    /* Clear the last row */
    for (int i = 0; i < VGA_WIDTH; i++) {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = make_entry(' ', VGA_COLOR);
    }
}

void tty_putchar(char c) {
    /* Also echo to serial port for headless debugging */
    serial_putchar(c);

    if (c == '\n') {
        col = 0;
        row++;
    } else if (c == '\r') {
        col = 0;
    } else if (c == '\b') {
        if (col > 0) {
            col--;
            vga[row * VGA_WIDTH + col] = make_entry(' ', VGA_COLOR);
        }
    } else if (c == '\t') {
        col = (col + 8) & ~7;   /* Align to next 8-column boundary */
    } else {
        vga[row * VGA_WIDTH + col] = make_entry(c, VGA_COLOR);
        col++;
    }

    /* Wrap at end of line */
    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
    }

    /* Scroll if we've gone past the bottom */
    if (row >= VGA_HEIGHT) {
        scroll();
        row = VGA_HEIGHT - 1;
    }

    update_cursor();
}

void tty_print(const char *str) {
    while (*str)
        tty_putchar(*str++);
}

void tty_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = make_entry(' ', VGA_COLOR);
    }
    col = 0;
    row = 0;
    update_cursor();
}

void tty_init(void) {
    tty_clear();
}
