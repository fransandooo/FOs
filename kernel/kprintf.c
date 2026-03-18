#include "kprintf.h"
#include "tty.h"
#include <stdarg.h>
#include <stdint.h>

static void kput(char c) {
    tty_putchar(c);     /* tty_putchar already echoes to serial */
}

static void kputs(const char *s) {
    if (!s) s = "(null)";
    while (*s) kput(*s++);
}

static void kprint_uint(uint64_t val, int base, int min_digits) {
    char buf[64];
    int i = 0;

    if (val == 0) {
        buf[i++] = '0';
    } else {
        while (val) {
            int digit = val % base;
            buf[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
            val /= base;
        }
    }

    while (i < min_digits) buf[i++] = '0';
    while (i > 0) kput(buf[--i]);
}

static void kprint_int(int64_t val) {
    if (val < 0) {
        kput('-');
        kprint_uint((uint64_t)(-val), 10, 1);
    } else {
        kprint_uint((uint64_t)val, 10, 1);
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            kput(*fmt++);
            continue;
        }
        fmt++;  /* skip '%' */

        /* Check for 'l' / 'll' length modifier */
        int is_ll = 0;
        if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') { is_ll = 1; fmt++; }
            else { is_ll = 1; }     /* on x86_64, long == long long */
        }

        switch (*fmt) {
        case 'd':
            if (is_ll) kprint_int(va_arg(args, int64_t));
            else       kprint_int(va_arg(args, int));
            break;
        case 'u':
            if (is_ll) kprint_uint(va_arg(args, uint64_t), 10, 1);
            else       kprint_uint(va_arg(args, unsigned), 10, 1);
            break;
        case 'x':
            if (is_ll) kprint_uint(va_arg(args, uint64_t), 16, 1);
            else       kprint_uint(va_arg(args, unsigned), 16, 1);
            break;
        case 'p':
            kputs("0x");
            kprint_uint((uint64_t)va_arg(args, void *), 16, 16);
            break;
        case 's':
            kputs(va_arg(args, const char *));
            break;
        case 'c':
            kput((char)va_arg(args, int));
            break;
        case '%':
            kput('%');
            break;
        default:
            kput('%');
            kput(*fmt);
            break;
        }
        fmt++;
    }

    va_end(args);
}
