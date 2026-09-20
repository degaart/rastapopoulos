#pragma once

#ifdef RASTA
#include <stdarg.h>

int putchar(int ch);
int printf(const char* fmt, ...);
int vprintf( const char* restrict format, va_list vlist );
#else
#include_next <stdio.h>
#endif


