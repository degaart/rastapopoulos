#include <stdio.h>

int serial_printf(const char* fmt, ...) ATTR_FORMAT(1, 2);
void _trace(const char* file, int line, const char* fmt, ...)
    ATTR_FORMAT(3, 4);

#define trace(...) _trace(__FILE__, __LINE__, __VA_ARGS__)
