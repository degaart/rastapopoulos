#include <stdarg.h>
#include "debug.h"
#include "io.h"
#include "util.h"

void trace_write(int ch, void* params) {
    serial_write_char(ch);
}

void trace(const char* file, unsigned line, const char* function, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    tracev(file, line, function, fmt, ap);
    va_end(ap);
}

void tracev(const char* file, unsigned line, const char* function, const char* fmt, va_list args) {
    format(trace_write, 0, "[%s:%u %s] ", file, line, function);
    formatv(trace_write, 0, fmt, args);
    serial_write_char('\n');
}

void halt() {
    while(1) {
        asm(
            "cli\n"
            "hlt\n"
        );
    }
}


