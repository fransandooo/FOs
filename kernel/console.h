#ifndef CONSOLE_H
#define CONSOLE_H

/*
 * Framebuffer text console — replaces VGA text mode output.
 *
 * Provides the same interface as tty (putchar, clear) but renders text
 * to the pixel framebuffer using the 8x16 bitmap font.
 */

/* Initialize the framebuffer console. Call after fb_init(). */
void console_init(void);

/* Write a single character (handles \n, \b, \t, \r, scrolling) */
void console_putchar(char c);

/* Clear the console and reset cursor */
void console_clear(void);

#endif
