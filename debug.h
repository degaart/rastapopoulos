#pragma once

#define TRACE(fmt, ...) trace(__FILE__, __LINE__, __PRETTY_FUNCTION__, fmt "\n", #__VA_ARGS__);
#define PANIC(fmt, ...) \
    do { \
        trace(__FILE__, __LINE__, __PRETTY_FUNCTION__, "PANIC: " fmt "\n", #__VA_ARGS__); \
        while(1); \
    } while(0)
#define assert(cond) if(!(cond)) PANIC("Assertion failed: " # cond);

void trace_init();
void trace(const char* file, int line, const char* fn, const char* fmt, ...);
void serial_write_char(char ch);
void serial_write_string(const char* s);


