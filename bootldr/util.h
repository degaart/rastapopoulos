#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdarg.h>

void itoa(char* str, unsigned n);
void itox(char* str, unsigned n);

typedef void (*write_callback_t)(int ch, void* params);
void format(write_callback_t callback, void* callback_params, const char* format, ...);
void formatv(write_callback_t callback, void* callback_params, const char* format, va_list args);

#define ALIGN32(value, alignment) \
    ( ( ( value ) + ( ( alignment ) - 1 ) ) & ~( ( alignment ) - 1 ) )


#endif //_UTIL_H_

