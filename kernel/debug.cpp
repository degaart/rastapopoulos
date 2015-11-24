#include <stdarg.h>
#include "debug.h"
#include "io.h"
#include "string.h"
#include "backtrace.h"

static void trace_write(int ch, void* params) {
    // while(!(IO::inb(0x3f8 + 5) & 0x20)); /* wait for queue empty */
    // IO::outb(0x3f8, ch & 0xFF);
    IO::outb(0xE9, ch);
}

void Debug::trace(const char* file, unsigned line, const char* function, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    tracev(file, line, function, fmt, ap);
    va_end(ap);
}

void Debug::tracev(const char* file, unsigned line, const char* function, const char* fmt, va_list args) {
    String::format(trace_write, 0, "[%s:%u %s] ", file, line, function);
    String::formatv(trace_write, 0, fmt, args);
    trace_write('\n', 0);
}

void Debug::panic(const char* file, unsigned line, const char* function, const char* fmt, ...) {
    String::format(trace_write, 0, "[%s:%u %s] PANIC: ", file, line, function);

    va_list args;
    va_start(args, fmt);
    String::formatv(trace_write, 0, fmt, args);
    va_end(args);
    trace_write('\n', 0);
    backtrace();
    halt();
}

void Debug::write_string(const char* str) {
    while(*str) {
        IO::outb(0xE9, *str);
        str++;
    }
}

void Debug::write_char(int ch) {
    IO::outb(0xE9, ch);
}