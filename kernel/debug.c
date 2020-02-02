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

static
void __logv(const char* func, const char* file, int line, const char* fmt, va_list args)
{
    const char* bname = basename(file);
    format(debug_write_char, NULL, "[%s:%d][%s] ", bname, line, func);
    formatv(debug_write_char, NULL, fmt, args);
    debug_write_char('\n', NULL);
}

void __log(const char* func, const char* file, int line, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    __logv(func, file, line, fmt, args);
    va_end(args);
}

void __assertion_failed(const char* func, const char* file, int line, const char* assertion, const char* fmt, ...)
{
    if(fmt) {
        va_list args;
        va_start(args, fmt);
        __logv(func, file, line, fmt, args);
        va_end(args);
    }

    trace("*** KERNEL PANIC ***");
    __log(func, file, line, "Assertion failed: %s", assertion);
    abort();
}

void abort()
{
    reboot();
}

