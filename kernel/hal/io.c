#include "io.h"

void serial_write(const char* s) {
    while (*s) {
        outb(0x3f8, *s++);
    }
}
void uint64_to_hex(uint64_t n, char* buf) {
    const char* hex = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex[n & 0xF];
        n >>= 4;
    }
    buf[16] = '\0';
}

#define va_list __builtin_va_list
#define va_start(v,l) __builtin_va_start(v,l)
#define va_arg(v,l) __builtin_va_arg(v,l)
#define va_end(v) __builtin_va_end(v)

void serial_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1) == 'p') {
            uint64_t p = va_arg(args, uint64_t);
            char buf[17];
            uint64_to_hex(p, buf);
            serial_write("0x");
            serial_write(buf);
            fmt += 2;
        } else if (*fmt == '%' && *(fmt + 1) == 'd') {
            // Very simple %d
            int d = va_arg(args, int);
            if (d == 0) {
                serial_write("0");
            } else {
                char buf[12];
                int i = 10;
                buf[11] = '\0';
                while (d > 0) {
                    buf[i--] = (d % 10) + '0';
                    d /= 10;
                }
                serial_write(&buf[i+1]);
            }
            fmt += 2;
        } else if (*fmt == '%' && *(fmt + 1) == 'x') {
            uint64_t x = va_arg(args, uint64_t);
            char buf[17];
            uint64_to_hex(x, buf);
            // Trim leading zeros for %x
            char* p = buf;
            while (*p == '0' && *(p+1) != '\0') p++;
            serial_write(p);
            fmt += 2;
        } else {
            char buf[2] = {*fmt++, '\0'};
            serial_write(buf);
        }
    }

    va_end(args);
}
