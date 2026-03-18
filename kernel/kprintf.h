#ifndef KPRINTF_H
#define KPRINTF_H

/*
 * Kernel printf — formatted output to VGA + serial.
 *
 * Supported format specifiers:
 *   %d    signed 32-bit decimal
 *   %u    unsigned 32-bit decimal
 *   %x    unsigned 32-bit hex (lowercase)
 *   %lld  signed 64-bit decimal
 *   %llu  unsigned 64-bit decimal
 *   %llx  unsigned 64-bit hex
 *   %p    pointer (0x + 16-digit hex)
 *   %s    string
 *   %c    character
 *   %%    literal percent
 */
void kprintf(const char *fmt, ...);

#endif
