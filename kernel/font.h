#ifndef FONT_H
#define FONT_H

#include <stdint.h>

/*
 * 8x16 bitmap font (CP437 character set).
 * 256 glyphs, 16 bytes per glyph (one byte per row, MSB = leftmost pixel).
 * Total: 4096 bytes.
 */
extern const uint8_t font8x16[256 * 16];

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

#endif
