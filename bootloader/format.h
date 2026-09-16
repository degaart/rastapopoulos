#pragma once

#include <stdarg.h>

typedef void (*FormatOutputFn)(void* data, char character);
int vformat(FormatOutputFn output, void* data, const char* fmt, va_list args);
int format(FormatOutputFn output, void* data, const char* fmt, ...);

#ifdef RASTACC
int printf(const char* fmt, ...);
#endif


