#pragma once

#include <stdarg.h>

typedef void (*FormatOutputFn)(void* data, char character);
int vformat(FormatOutputFn output, void* data, const char* fmt, va_list args);
int format(FormatOutputFn output, void* data, const char* fmt, ...) __attribute__((format(printf, 3, 4)));

