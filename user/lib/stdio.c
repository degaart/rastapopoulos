#include "stdio.h"
#include "rasta.h"

#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

static char* sprintfcb(const char* buf, void* user, int len)
{
    sys_write(buf, len);
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

