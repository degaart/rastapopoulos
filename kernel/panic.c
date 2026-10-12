#include "kernel.h"
#include "serial.h"
#include "trace.h"
#include "vga.h"
#include <stb/stb_sprintf.h>

void __panic(const char* file, int line, const char* fmt, ...)
{
    serial_printf("[%s:%d] KERNEL PANIC -- ", file, line);

    va_list args;
    va_start(args, fmt);
    serial_vprintf(fmt, args);
    va_end(args);

    printf("[%s:%d] KERNEL PANIC -- ", file, line);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    halt();
}

