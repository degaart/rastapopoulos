#include "trace.h"
#include "serial.h"
#include "vga.h"
#include <stb/stb_sprintf.h>
#include <stdarg.h>

static char* serialcb(const char* buf, void* user, int len)
{
    serial_write_string(buf, len);
    return (char*)buf;
}

int serial_printf(const char* fmt, ...)
{
    char buf[STB_SPRINTF_MIN];
    va_list args;
    va_start(args, fmt);
    int ret = stbsp_vsprintfcb(serialcb, NULL, buf, fmt, args);
    va_end(args);
    return ret;
}

int serial_vprintf(const char* fmt, va_list args)
{
    char buf[STB_SPRINTF_MIN];
    int ret = stbsp_vsprintfcb(serialcb, NULL, buf, fmt, args);
    return ret;
}

void vtrace(const char* file, int line, const char* fmt, va_list args)
{
    serial_printf("[%s:%d] ", file, line);

    char buf[STB_SPRINTF_MIN];
    stbsp_vsprintfcb(serialcb, NULL, buf, fmt, args);
    serial_putchar('\n');
}

void _trace(const char* file, int line, const char* fmt, ...)
{
    serial_printf("[%s:%d] ", file, line);

    char buf[STB_SPRINTF_MIN];
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintfcb(serialcb, NULL, buf, fmt, args);
    va_end(args);

    serial_putchar('\n');
}

