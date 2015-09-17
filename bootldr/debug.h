#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "util.h"

void halt(); 
void tracev(const char* file, unsigned line, const char* function, const char* format, va_list args);
void trace(const char* file, unsigned line, const char* function, const char* fmt, ...);

#define TRACE(...) trace(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BREAKPOINT() \
    asm volatile( \
        ".intel_syntax noprefix\n" \
        "xchg bx, bx\n" \
    )

#endif //_DEBUG_H_
