// compat.c — stub implementations of glibc 2.38+ C23 symbols.
// Needed when linking against a raylib binary built with GCC 14+
// on a system with GCC 13 or older.
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int __isoc23_sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(str, fmt, ap);
    va_end(ap);
    return r;
}

long __isoc23_strtol(const char *nptr, char **endptr, int base) {
    return strtol(nptr, endptr, base);
}

unsigned long __isoc23_strtoul(const char *nptr, char **endptr, int base) {
    return strtoul(nptr, endptr, base);
}
