#include "tty.h"
#include "serial.h"
#include "io.h"
#include "console.h"
#include <stdint.h>

/*
 * TTY Output Dispatcher
 *
 * Routes character output to either VGA text mode (0xB8000) or the
 * framebuffer console, depending on which display mode is active.
 * Serial output is always echoed regardless of display mode.
 */

#define VGA_ADDRESS  0xB8000
#define VGA_WIDTH    80
#define VGA_HEIGHT   25
#define VGA_COLOR    0x0F        /* White text on black background */

static volatile uint16_t *vga = (volatile uint16_t *)VGA_ADDRESS;
static int col = 0;
static int row = 0;
static int graphics_mode = 0;   /* 0 = VGA text, 1 = framebuffer console */

static uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/* Update VGA hardware cursor position */
static void update_cursor(void) {
    uint16_t pos = row * VGA_WIDTH + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* Scroll VGA text buffer up by one line */
static void vga_scroll(void) {
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++)
        vga[i] = vga[i + VGA_WIDTH];
    for (int i = 0; i < VGA_WIDTH; i++)
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = make_entry(' ', VGA_COLOR);
}

/* Write character to VGA text buffer */
static void vga_putchar(char c) {
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
        col = (col + 8) & ~7;
    } else {
        vga[row * VGA_WIDTH + col] = make_entry(c, VGA_COLOR);
        col++;
    }

    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
    }
    if (row >= VGA_HEIGHT) {
        vga_scroll();
        row = VGA_HEIGHT - 1;
    }
    update_cursor();
}

void tty_putchar(char c) {
    serial_putchar(c);

    if (graphics_mode)
        console_putchar(c);
    else
        vga_putchar(c);
}

void tty_print(const char *str) {
    while (*str)
        tty_putchar(*str++);
}

void tty_clear(void) {
    if (graphics_mode) {
        console_clear();
    } else {
        for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
            vga[i] = make_entry(' ', VGA_COLOR);
        col = 0;
        row = 0;
        update_cursor();
    }
}

void tty_init(void) {
    graphics_mode = 0;
    tty_clear();
}

void tty_set_graphics(void) {
    graphics_mode = 1;
}
