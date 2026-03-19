#ifndef VBE_H
#define VBE_H

#include <stdint.h>

/*
 * VBE (VESA BIOS Extensions) Mode Information Block
 *
 * Populated by Stage 2 via int 0x10, AX=0x4F01 and stored at VBE_INFO_ADDR.
 * The framebuffer physical address and pitch are the critical fields.
 */
typedef struct {
    uint16_t attributes;
    uint8_t  window_a, window_b;
    uint16_t granularity, window_size;
    uint16_t segment_a, segment_b;
    uint32_t win_func_ptr;
    uint16_t pitch;             /* Bytes per scanline */
    uint16_t width;             /* Horizontal resolution in pixels */
    uint16_t height;            /* Vertical resolution in pixels */
    uint8_t  w_char, y_char, planes, bpp, banks;
    uint8_t  memory_model, bank_size, image_pages;
    uint8_t  reserved0;
    uint8_t  red_mask, red_pos;
    uint8_t  green_mask, green_pos;
    uint8_t  blue_mask, blue_pos;
    uint8_t  rsv_mask, rsv_pos;
    uint8_t  directcolor_attrs;
    uint32_t framebuffer;       /* Physical address of linear framebuffer */
    uint32_t off_screen_mem;
    uint16_t off_screen_mem_size;
    uint8_t  reserved1[206];
} __attribute__((packed)) VBEModeInfo;

#define VBE_INFO_ADDR   0x6000
#define VBE_MAGIC_ADDR  0x5FF0
#define VBE_MAGIC       0x1EAF  /* Stored by Stage 2 on VBE success */

#endif
