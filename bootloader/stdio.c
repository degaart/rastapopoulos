#include "stdio.h"
#include <stdarg.h>
#include "format.h"
#include <stddef.h>

int putchar(int ch)
{
    asm volatile("cmp al, 10\n\t"
                 "jne 1f\n\t"
                 "push ax\n\t"
                 "mov al, 13\n\t"
                 "mov ah, 0xe\n\t"
                 "mov bx, 0x7\n\t"
                 "int 0x10\n\t"
                 "pop ax\n\t"
                 "1:\n\t"
                 "mov ah, 0xe\n\t"
                 "mov bx, 0x7\n\t"
                 "int 0x10"
                 : "+a"(ch)
                 :
                 : "bx", "bp", "cc", "memory");
    return ch;
}

static void format_out(void* unused, char ch)
{
    putchar(ch);
}

int printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vformat(format_out, NULL, fmt, args);
    va_end(args);
    return ret;
}

int vprintf( const char* restrict format, va_list vlist )
{
    return vformat(format_out, NULL, format, vlist);
}


