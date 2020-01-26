#pragma once

#define panic(...) \
    do {                                                                \
        __log(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__);           \
        abort();                                                        \
    } while(0)

#define trace(...) \
    __log(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)

#define assert(c)                                                       \
    do {                                                                \
        if(!(c)) {                                                      \
            panic("Assertion failed: " #c);                             \
        }                                                               \
    } while(0)

void __log(const char* func, const char* file, int line, const char* fmt, ...);
void abort() __attribute__((noreturn));
void debug_write(const char* str);


