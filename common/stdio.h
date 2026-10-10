#pragma once

#include <attr_format.h>
#include <stdarg.h>
#include <stddef.h>

int putchar(int ch);
int printf(const char* fmt, ...) ATTR_FORMAT(1, 2);
int vprintf(const char* restrict format, va_list vlist);
int snprintf(char* restrict buffer, size_t bufsz, const char* restrict format,
             ...) ATTR_FORMAT(3, 4);

