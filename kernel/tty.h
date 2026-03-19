#ifndef TTY_H
#define TTY_H

/* Initialize the display (VGA text mode by default) */
void tty_init(void);

/* Switch output to framebuffer graphics console */
void tty_set_graphics(void);

/* Clear the screen and reset cursor */
void tty_clear(void);

/* Print a single character to the screen */
void tty_putchar(char c);

/* Print a null-terminated string to the screen */
void tty_print(const char *str);

#endif
