#include "trace.h"
#include "serial.h"
#include <stb/stb_sprintf.h>
#include <stdarg.h>

static char* tracecb(const char* buf, void* user, int len)
{
    const char* ptr = buf;
    while (len) {
        serial_putchar(*ptr);
        ptr++;
        len--;
    }
    return (char*)buf;
}

int serial_printf(const char* fmt, ...)
{
    char buf[STB_SPRINTF_MIN];
    va_list args;
    va_start(args, fmt);
    int ret = stbsp_vsprintfcb(tracecb, NULL, buf, fmt, args);
    va_end(args);
    return ret;
}

void _trace(const char* file, int line, const char* fmt, ...)
{
    serial_printf("[%s:%d] ", file, line);

    char buf[STB_SPRINTF_MIN];
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintfcb(tracecb, NULL, buf, fmt, args);
    va_end(args);

    serial_putchar('\n');
}

