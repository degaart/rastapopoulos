#include "runtime.h"
#include "syscalls.h"
#include "io.h"
#include "string.h"
#include <stdarg.h>

void yield()
{
    syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
}

int fork()
{
    int pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
    return pid;
}

void user_sleep(unsigned ms)
{
    syscall(SYSCALL_SLEEP,
            ms,
            0,
            0,
            0,
            0);
}

void user_exit()
{
    syscall(SYSCALL_EXIT,
            0,
            0,
            0,
            0,
            0);
}

void setname(const char* new_name)
{
    syscall(SYSCALL_SETNAME, (uint32_t)new_name, 0, 0, 0, 0);
}

void abort()
{
    reboot();
}

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

void __assertion_failed(const char* function, const char* file, int line, const char* expression)
{
    __log(function, file, line, "Assertion failed: %s", expression);
    abort();
}

void reboot()
{
    syscall(SYSCALL_REBOOT,
            0,
            0,
            0,
            0,
            0);
}


