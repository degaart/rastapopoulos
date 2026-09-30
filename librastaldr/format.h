#pragma once

#include <stdarg.h>
#include "attr_format.h"

typedef void (*FormatOutputFn)(void* data, char character);
int vformat(FormatOutputFn output, void* data, const char* fmt, va_list args);
int format(FormatOutputFn output, void* data, const char* fmt, ...) ATTR_FORMAT(3, 4);
int printf(const char* fmt, ...) ATTR_FORMAT(1, 2);

