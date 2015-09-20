#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdarg.h>
#include "util.h"

class Debug {
public:
	static void tracev(const char* file, unsigned line, const char* function, const char* format, va_list args);
	static void trace(const char* file, unsigned line, const char* function, const char* fmt, ...) __attribute__ ((format (printf, 4, 5)));
    static void panic(const char* file, unsigned line, const char* function, const char* fmt, ...) __attribute__((format (printf, 4, 5)));
};

#define TRACE(...) Debug::trace(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BREAKPOINT() \
    asm volatile( \
        ".intel_syntax noprefix\n" \
        "xchg bx, bx\n" \
    )

#define assert(cond) \
    while(!(cond)) { \
        PANIC("Assertion failed:\n\t%s", #cond); \
        halt(); \
    }

#define ASSERT(conde) assert(cond)

#define PANIC(...) Debug::panic(__FILE__, __LINE__, __func__, __VA_ARGS__)

#endif //_DEBUG_H_
