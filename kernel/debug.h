#pragma once

#define panic(...) \
    do {                                                                \
        trace("*** KERNEL PANIC ***");                                  \
        __log(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__);           \
        abort();                                                        \
    } while(0)

#define trace(...) \
    __log(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)

#define assert2(c, ...)                                                                 \
    do {                                                                                \
        if(!(c)) {                                                                      \
            __assertion_failed(__FUNCTION__, __FILE__, __LINE__, #c, __VA_ARGS__);      \
        }                                                                               \
    } while(0)

#define assert(c) assert2(c, NULL)

void __assertion_failed(const char* func, const char* file, int line, const char* assertion, const char* fmt, ...);
void __log(const char* func, const char* file, int line, const char* fmt, ...); /*__attribute__((format(printf,4,5)));*/
void abort() __attribute__((noreturn));
void debug_write(const char* str);


