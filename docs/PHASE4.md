# Phase 4: Framebuffer Graphics

## Overview

Phase 4 replaces VGA text mode with a pixel framebuffer obtained via VBE
(VESA BIOS Extensions). Every character on screen is now drawn by the kernel
using an embedded 8x16 bitmap font. The OS looks like an OS.

## Architecture

```
Stage 2 bootloader (real mode)
   |  VBE: enumerate modes, select best 32bpp mode, set it
   |  Mode info stored at 0x6000 (framebuffer address, pitch, resolution)
   v
Kernel fb_init()
   |  Read VBE mode info from 0x6000
   |  Identity-map framebuffer pages via vmm_map_page()
   v
Framebuffer driver (framebuffer.c)
   |  Pixel primitives: put_pixel, draw_rect, draw_char, scroll
   v
Console layer (console.c)
   |  Text rendering: newline, backspace, tab, scrolling
   v
TTY dispatcher (tty.c)
   |  Routes output to framebuffer console or VGA text (fallback)
   |  Serial output always echoed
   v
kprintf() — unchanged
```

## VBE Mode Setup (Stage 2)

VBE mode selection happens in 16-bit real mode because it uses BIOS int 0x10.
It's the last step before entering protected mode, after all BIOS text output
is finished (since graphics mode disables the text display).

### Mode Enumeration

Instead of trying hardcoded mode numbers (which turned out to be 24bpp on
QEMU's standard VGA), we enumerate all VBE modes:

1. Call VBE 0x4F00 with "VBE2" signature to get controller info
2. Read the mode list pointer (far pointer at offset 14 in controller info)
3. For each mode in the list (terminated by 0xFFFF):
   - Call VBE 0x4F01 to get mode info
   - Check: linear framebuffer support (attribute bit 7)
   - Check: 32bpp
   - Check: resolution between 640x480 and 1920x1200
4. Pick the mode with the most pixels (highest resolution)
5. Call VBE 0x4F02 to set it with the linear framebuffer bit (bit 14)

On QEMU with default settings, this selects 1600x900x32bpp.

### Data Passed to Kernel

- `0x5FF0` (VBE_MAGIC_ADDR): `0x1EAF` on success, `0x0000` on failure
- `0x6000` (VBE_INFO_ADDR): 256-byte VBEModeInfo struct with:
  - `framebuffer` (offset 40): physical address of the linear framebuffer
  - `width` (offset 18), `height` (offset 20): resolution
  - `pitch` (offset 16): bytes per scanline
  - `bpp` (offset 25): bits per pixel (should be 32)

## Framebuffer Mapping

The framebuffer physical address (e.g., 0xFD000000 on QEMU) is in PCI MMIO
space, above our identity-mapped RAM range (~128MB). We identity-map it using
`vmm_map_page()` — one 4KB page at a time.

This works because `vmm_map_page()` creates new page table entries (PD → PT)
for PDPT indices that weren't used by the initial 2MB huge page mapping. No
conflict with existing mappings.

Pages are mapped with PWT (Page Write-Through) flag for better MMIO
performance.

## Components

### Framebuffer Driver (`kernel/framebuffer.c`)

Pixel-level primitives:
- `fb_put_pixel(x, y, color)` — single pixel with bounds checking
- `fb_clear(color)` — fill entire screen
- `fb_draw_rect(x, y, w, h, color)` — filled rectangle
- `fb_draw_rect_outline(x, y, w, h, color)` — 1px border rectangle
- `fb_draw_char(x, y, c, fg, bg)` — render 8x16 glyph
- `fb_draw_string(x, y, s, fg, bg)` — render string
- `fb_scroll_up(pixels, color)` — scroll framebuffer up, clear bottom

All pixel access uses `pitch` (bytes per scanline) for row addressing, not
`width * 4`. This handles hardware padding correctly.

### Bitmap Font (`kernel/font.c`)

Standard VGA BIOS 8x16 font (CP437 character set). 256 glyphs, 16 bytes per
glyph. MSB = leftmost pixel. 4096 bytes of read-only data.

Includes full ASCII, box-drawing characters, shade blocks, and Latin
extended characters.

### Framebuffer Console (`kernel/console.c`)

Text console built on the framebuffer driver:
- Tracks cursor position in character cells (col, row)
- Handles `\n`, `\r`, `\b`, `\t`
- Scrolls via `fb_scroll_up()` when cursor passes bottom of screen
- Colors: soft green text on near-black blue background

### TTY Dispatcher (`kernel/tty.c`)

Modified to dispatch between two output backends:
- **VGA text mode** (default): direct writes to 0xB8000, hardware cursor
- **Framebuffer console**: pixel rendering via `console_putchar()`

`tty_set_graphics()` switches to framebuffer mode after `fb_init()` and
`console_init()` succeed. Serial output is always echoed regardless of mode.

If VBE fails, the kernel falls back to VGA text mode transparently.

### Shell Updates

New `demo` command: 500-frame bouncing box animation (60x60 cyan rectangle
with white border bouncing around the screen at ~100fps).

## Improvements Over Plan

1. **VBE mode enumeration** instead of hardcoded mode numbers. QEMU's standard
   VGA reports standard VESA modes as 24bpp. Proper enumeration finds
   vendor-specific 32bpp modes that the hardcoded approach would miss.

2. **Best-mode selection**: picks the highest-resolution 32bpp mode within
   sane bounds (640x480 to 1920x1200), rather than a fixed resolution.

3. **VGA text fallback**: if VBE fails entirely, the kernel continues in
   VGA text mode. No hard dependency on graphics.

4. **Identity-mapped framebuffer**: uses virt==phys mapping instead of a
   separate virtual address range. Simpler and avoids managing a separate
   address space region.

5. **Added memmove** to string library for overlapping memory copies
   (used by scroll operations).

## Memory Layout Update

```
0x300000              PMM bitmap
0x400000 - 0x43FFFF   Kernel heap (initial 256 KB)
...
0xFD000000+           Framebuffer (identity-mapped, MMIO)
```

## Test Results

```
FB: 1600x900 32bpp pitch=6400 phys=0xfd000000
Timer: 10 ticks in ~100ms PASS

FOs> help
Commands: help, ticks, mem, clear, demo
FOs> mem
Free: 126 MB (32401 pages)
```

Boot screen with centered "FOs" title displays for 1.5 seconds before
transitioning to the console. Bouncing box demo runs smoothly at ~100fps.
