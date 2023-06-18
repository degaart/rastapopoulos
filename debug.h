#pragma once

#define TRACE(...) trace(__FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define PANIC(...) \
    do { \
        trace(__FILE__, __LINE__, __PRETTY_FUNCTION__, "*** KERNEL PANIC ***"); \
        trace(__FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__); \
        while(1); \
    } while(0)
#define assert(cond) if(!(cond)) PANIC("Assertion failed: " # cond);

void trace_init();
void trace(const char* file, int line, const char* fn, const char* fmt, ...);
void serial_write_char(char ch);
void serial_write_string(const char* s);

