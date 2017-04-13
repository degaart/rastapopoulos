#pragma once

#define DEBUG_PORT 0xE9

#define panic(...) \
    do {                                                                \
        __log(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__);           \
        abort();                                                        \
    } while(0)

#define breakpoint() __asm__ volatile("xchgw %bx, %bx")
#define invalid_code_path() panic("Invalid code path")

#define trace(...) \
    __log(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)

#define dump_var(var) \
    trace("%s: %p", #var, var)

#define assert(c)                                                       \
    do {                                                                \
        if(!(c)) {                                                      \
            __assertion_failed(__FUNCTION__, __FILE__, __LINE__, #c);   \
        }                                                               \
    } while(0)

void __assertion_failed(const char* function, const char* file, int line, const char* expression);
void __log(const char* func, const char* file, int line, const char* fmt, ...);
void abort();

