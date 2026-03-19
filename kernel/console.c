#include "console.h"
#include "framebuffer.h"
#include "font.h"
#include <stdint.h>

/*
 * Framebuffer Text Console
 *
 * Renders text to the pixel framebuffer using the 8x16 bitmap font.
 * Supports scrolling, newline, backspace, tab, and carriage return.
 *
 * Scrolling uses fb_scroll_up() which memmoves ~3MB of pixel data at
 * 1024x768. Noticeable but acceptable for Phase 4.
 */

#define CONSOLE_BG  RGB(15,  15,  25)    /* Near-black blue */
#define CONSOLE_FG  RGB(200, 230, 200)   /* Soft green */

static uint32_t cols, rows;
static uint32_t cursor_col, cursor_row;

void console_init(void) {
    cols = fb_get_width() / FONT_WIDTH;
    rows = fb_get_height() / FONT_HEIGHT;
    cursor_col = 0;
    cursor_row = 0;
    fb_clear(CONSOLE_BG);
}

void console_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            fb_draw_char(
                cursor_col * FONT_WIDTH,
                cursor_row * FONT_HEIGHT,
                ' ', CONSOLE_FG, CONSOLE_BG
            );
        }
    } else if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7u;
        if (cursor_col >= cols) {
            cursor_col = 0;
            cursor_row++;
        }
    } else {
        fb_draw_char(
            cursor_col * FONT_WIDTH,
            cursor_row * FONT_HEIGHT,
            c, CONSOLE_FG, CONSOLE_BG
        );
        cursor_col++;
        if (cursor_col >= cols) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= rows) {
        fb_scroll_up(FONT_HEIGHT, CONSOLE_BG);
        cursor_row = rows - 1;
    }
}

void console_clear(void) {
    fb_clear(CONSOLE_BG);
    cursor_col = 0;
    cursor_row = 0;
}
