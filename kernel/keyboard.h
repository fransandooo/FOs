#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/* Initialize keyboard driver (registers IRQ1 handler) */
void keyboard_init(void);

/* Get next character from input buffer (0 if empty, non-blocking) */
char keyboard_getchar(void);

#endif
