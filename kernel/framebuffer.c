#include "framebuffer.h"
#include "vbe.h"
#include "vmm.h"
#include "font.h"
#include "kprintf.h"
#include "string.h"
#include <stdint.h>

/*
 * Framebuffer Graphics Driver
 *
 * Uses a linear framebuffer obtained via VBE (set up by Stage 2).
 * The framebuffer is at a physical address in PCI MMIO space (typically
 * 0xFD000000 on QEMU), which is above our identity-mapped RAM range.
 * We identity-map it page-by-page using vmm_map_page().
 *
 * Pixel format: 32bpp BGRA (blue at lowest byte address on most VBE
 * implementations). Our RGB() macro packs as 0x00RRGGBB which works
 * because VBE reports red_pos=16, green_pos=8, blue_pos=0.
 */

static uint8_t  *fb_base;
static uint32_t  fb_width;
static uint32_t  fb_height;
static uint32_t  fb_pitch;     /* Bytes per scanline (may be > width*4) */
static uint32_t  fb_bpp;

int fb_init(void) {
    if (*(uint16_t *)VBE_MAGIC_ADDR != VBE_MAGIC)
        return 0;

    VBEModeInfo *info = (VBEModeInfo *)VBE_INFO_ADDR;
    fb_width  = info->width;
    fb_height = info->height;
    fb_pitch  = info->pitch;
    fb_bpp    = info->bpp;

    uint64_t fb_phys = info->framebuffer;
    uint32_t fb_size = fb_pitch * fb_height;

    /*
     * Identity-map the framebuffer: virtual address == physical address.
     * The framebuffer is in PCI MMIO space (e.g., 0xFD000000), well above
     * our 128MB RAM identity mapping. vmm_map_page will create new page
     * table entries without conflicting with the existing 2MB huge pages.
     *
     * We use write-combining flags (PWT) for better framebuffer performance.
     */
    uint32_t pages = (fb_size + 0xFFF) / 0x1000;
    for (uint32_t i = 0; i < pages; i++) {
        vmm_map_page(
            fb_phys + i * 0x1000,
            fb_phys + i * 0x1000,
            VMM_WRITE | (1ULL << 3)   /* PWT: write-through for MMIO */
        );
    }

    fb_base = (uint8_t *)fb_phys;

    /* Clear to black */
    fb_clear(COLOR_BLACK);

    kprintf("FB: %dx%d %dbpp pitch=%d phys=0x%llx\n",
            fb_width, fb_height, fb_bpp, fb_pitch, fb_phys);

    return 1;
}

static inline uint32_t *pixel_ptr(uint32_t x, uint32_t y) {
    return (uint32_t *)(fb_base + y * fb_pitch + x * 4);
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x < fb_width && y < fb_height)
        *pixel_ptr(x, y) = color;
}

void fb_clear(uint32_t color) {
    for (uint32_t y = 0; y < fb_height; y++) {
        uint32_t *row = (uint32_t *)(fb_base + y * fb_pitch);
        for (uint32_t x = 0; x < fb_width; x++)
            row[x] = color;
    }
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t row = y; row < y + h && row < fb_height; row++) {
        uint32_t *line = (uint32_t *)(fb_base + row * fb_pitch);
        uint32_t end = x + w;
        if (end > fb_width) end = fb_width;
        for (uint32_t col = x; col < end; col++)
            line[col] = color;
    }
}

void fb_draw_rect_outline(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    fb_draw_rect(x,         y,         w, 1, color);     /* Top */
    fb_draw_rect(x,         y + h - 1, w, 1, color);     /* Bottom */
    fb_draw_rect(x,         y,         1, h, color);     /* Left */
    fb_draw_rect(x + w - 1, y,         1, h, color);     /* Right */
}

void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph = &font8x16[(uint8_t)c * FONT_HEIGHT];

    for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
        uint32_t py = y + row;
        if (py >= fb_height) break;
        uint8_t bits = glyph[row];
        uint32_t *line = (uint32_t *)(fb_base + py * fb_pitch);
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            uint32_t px = x + col;
            if (px >= fb_width) break;
            line[px] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

void fb_draw_string(uint32_t x, uint32_t y, const char *s, uint32_t fg, uint32_t bg) {
    uint32_t cx = x;
    while (*s) {
        if (*s == '\n') {
            cx = x;
            y += FONT_HEIGHT;
        } else {
            fb_draw_char(cx, y, *s, fg, bg);
            cx += FONT_WIDTH;
        }
        s++;
    }
}

void fb_scroll_up(uint32_t pixels, uint32_t clear_color) {
    if (pixels >= fb_height) {
        fb_clear(clear_color);
        return;
    }
    /* Move pixel rows up */
    uint32_t copy_rows = fb_height - pixels;
    for (uint32_t y = 0; y < copy_rows; y++)
        memcpy(fb_base + y * fb_pitch,
               fb_base + (y + pixels) * fb_pitch,
               fb_pitch);
    /* Clear the vacated bottom rows */
    for (uint32_t y = copy_rows; y < fb_height; y++) {
        uint32_t *row = (uint32_t *)(fb_base + y * fb_pitch);
        for (uint32_t x = 0; x < fb_width; x++)
            row[x] = clear_color;
    }
}

uint32_t fb_get_width(void)  { return fb_width; }
uint32_t fb_get_height(void) { return fb_height; }
uint32_t fb_get_pitch(void)  { return fb_pitch; }
