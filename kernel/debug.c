#include "kernel.h"
#include "debug.h"
#include "string.h"
#include "io.h"

static void __log_callback(int ch, void* unused)
{
    outb(0xE9, ch);
}

void __log(const char* func, const char* file, int line, const char* fmt, ...)
{
    format(__log_callback, NULL, "[%s:%d][%s] ", file, line, func);

    va_list args;
    va_start(args, fmt);
    format(__log_callback, NULL, fmt, args);
    va_end(args);

    __log_callback('\n', NULL);
}

