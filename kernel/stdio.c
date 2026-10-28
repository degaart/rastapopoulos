#include "kernel.h"
#include "trace.h"
#include "vga.h"
#include <stdio.h>

#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>

static char* sprintfcb(const char* buf, void* user, int len)
{
    vga_write(buf, len, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    return (char*)buf;
}

int printf(const char* fmt, ...)
{
    char buf[STB_SPRINTF_MIN];

    va_list args;
    va_start(args, fmt);
    int ret = stbsp_vsprintfcb(sprintfcb, NULL, buf, fmt, args);
    va_end(args);
    return ret;
}

int vprintf(const char* restrict fmt, va_list args)
{
    char buf[STB_SPRINTF_MIN];
    return stbsp_vsprintfcb(sprintfcb, NULL, buf, fmt, args);
}

int snprintf(char* restrict buffer, size_t bufsz, const char* restrict format,
             ...)
{
    va_list args;
    va_start(args, format);
    int result = stbsp_vsnprintf(buffer, bufsz, format, args);
    va_end(args);
    return result;
}

