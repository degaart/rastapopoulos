#pragma once

#define trace(...) \
    __log(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)

#define assert(c)                                                       \
    do {                                                                \
        if(!(c)) {                                                      \
            __assertion_failed(__FUNCTION__, __FILE__, __LINE__, #c);   \
        }                                                               \
    } while(0)

#define breakpoint() __asm__ volatile("xchgw %bx, %bx")

#define DEBUG_PORT 0xE9

struct multiboot_info;

void backtrace();
void load_symbols(const struct multiboot_info* multiboot_info);
void __log(const char* func, const char* file, int line, const char* fmt, ...);
void __assertion_failed(const char* function, const char* file, int line, const char* expression);

