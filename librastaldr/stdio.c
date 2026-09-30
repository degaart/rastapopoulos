#include "stdio.h"
#include <stdarg.h>
#include "format.h"
#include <stddef.h>

static void format_out(void* unused, char ch)
{
    putchar(ch);
}

int printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vformat(format_out, NULL, fmt, args);
    va_end(args);
    return ret;
}

int vprintf( const char* restrict format, va_list vlist )
{
    return vformat(format_out, NULL, format, vlist);
}


