#include <string.h>

void format(format_callback_t callback, void* callback_params, const char* fmt,...) {
    va_list args;

    va_start(args, fmt);
    formatv(callback, callback_params, fmt, args);
    va_end(args);
}

