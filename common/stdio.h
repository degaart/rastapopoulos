#pragma once

#include <stdarg.h>

#ifdef RASTA_KERNEL
#    define ATTR_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#    define ATTR_FORMAT(fmt, args)
#endif

int putchar(int ch);
int printf(const char* fmt, ...) ATTR_FORMAT(1, 2);
int vprintf(const char* restrict format, va_list vlist);

