#ifndef _UTIL_H_
#define _UTIL_H_

void itoa(char* str, unsigned n);
void itox(char* str, unsigned n);

typedef void (*write_callback_t)(int ch, void* params);
typedef char* va_list;

void format(write_callback_t callback, void* callback_params, const char* format, ...);
void formatv(write_callback_t callback, void* callback_params, const char* format, va_list args);

#endif //_UTIL_H_

