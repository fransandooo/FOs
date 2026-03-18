#ifndef TTY_H
#define TTY_H

/* Initialize the VGA text mode display (clear screen, reset cursor) */
void tty_init(void);

/* Clear the screen and reset cursor to top-left */
void tty_clear(void);

/* Print a single character to the screen */
void tty_putchar(char c);

/* Print a null-terminated string to the screen */
void tty_print(const char *str);

#endif
