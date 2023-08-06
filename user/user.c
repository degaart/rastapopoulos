#include "user.h"
#include <stdarg.h>
#include <string.h>

uint32_t syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    asm volatile("int 0x30\n" : "+a"(eax) : "b"(ebx), "c"(ecx), "d"(edx));
    return eax;
}

void trace(const char* file, int line, const char* fn, const char* fmt, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    debug_write(buffer);
}

void panic(const char* file, int line, const char* fn, const char* fmt, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    debug_write(buffer);
    syscall(2, 0, 0, 0);
}

void debug_write(const char* msg)
{
    syscall(0, (uintptr_t)msg, 0, 0);
}

int add(int a, int b, int c)
{
    return syscall(1, a, b, c);
}

void halt()
{
    syscall(3, 0, 0, 0);
}

uint32_t get_ticks()
{
    return syscall(4, 0, 0, 0);
}
