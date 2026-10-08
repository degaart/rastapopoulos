#include "kernel.h"
#include "vga.h"
#include <stdio.h>

#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

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

void __panic(const char* file, int line, const char* fmt, ...)
{
    printf("PANIC at %s:%d -- ", file, line);

    char buf[STB_SPRINTF_MIN];
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintfcb(sprintfcb, NULL, buf, fmt, args);
    va_end(args);

    halt();
}

