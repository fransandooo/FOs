#ifndef SERIAL_H
#define SERIAL_H

/* Initialize COM1 serial port (38400 baud, 8N1) */
void serial_init(void);

/* Write a single character to the serial port */
void serial_putchar(char c);

/* Write a null-terminated string to the serial port */
void serial_print(const char *str);

#endif
