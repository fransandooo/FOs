#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

/* Color packing: 0x00RRGGBB */
#define RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

#define COLOR_BLACK   RGB(0,   0,   0)
#define COLOR_WHITE   RGB(255, 255, 255)
#define COLOR_RED     RGB(255, 0,   0)
#define COLOR_GREEN   RGB(0,   255, 0)
#define COLOR_BLUE    RGB(0,   0,   255)
#define COLOR_CYAN    RGB(0,   255, 255)
#define COLOR_YELLOW  RGB(255, 255, 0)
#define COLOR_GRAY    RGB(128, 128, 128)

/* Initialize framebuffer from VBE mode info. Returns 1 on success, 0 on failure. */
int fb_init(void);

/* Clear entire screen to a color */
void fb_clear(uint32_t color);

/* Draw a single pixel */
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);

/* Draw a filled rectangle */
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/* Draw a rectangle outline (1px border) */
void fb_draw_rect_outline(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/* Draw a single character using the 8x16 bitmap font */
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);

/* Draw a null-terminated string */
void fb_draw_string(uint32_t x, uint32_t y, const char *s, uint32_t fg, uint32_t bg);

/* Scroll the entire framebuffer up by `pixels` rows and clear the bottom */
void fb_scroll_up(uint32_t pixels, uint32_t clear_color);

uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
uint32_t fb_get_pitch(void);

#endif
