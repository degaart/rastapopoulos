#include "syscalls.h"
#include "io.h"
#include "string.h"
#include "debug.h"

static void trace_callback(int ch, void* args)
{
    outb(0xE9, ch);
}

void __log(const char* func, const char* file, int line, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    format(trace_callback, NULL, "[%s:%d][%s][init] ", file, line, func);
    formatv(trace_callback, NULL, fmt, ap);
    trace_callback('\n', NULL);

    va_end(ap);
}

void main()
{
    trace("Hello, from userland");
    syscall(SYSCALL_REBOOT,
            0,
            0,
            0,
            0,
            0);
}

