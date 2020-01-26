#include "debug.h"
#include "halt.h"
#include "string.h"
#include "io.h"
#include "reboot.h"

static void debug_write_char(int ch, void* unused)
{
    outb(IOPORT_DEBUG, ch);
}

void debug_write(const char* str)
{
    while(*str) {
        debug_write_char(*str, NULL);
        str++;
    }
}

void __log(const char* func, const char* file, int line, const char* fmt, ...)
{
    const char* bname = basename(file);

    format(debug_write_char, NULL, "[%s:%d][%s] ", bname, line, func);

    va_list args;
    va_start(args, fmt);
    format(debug_write_char, NULL, fmt, args);
    va_end(args);

    debug_write_char('\n', NULL);
}

void abort()
{
    reboot();
}

