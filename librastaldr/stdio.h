#pragma once

#include <stdarg.h>

int putchar(int ch);
int printf(const char* fmt, ...);
int vprintf( const char* restrict format, va_list vlist );

