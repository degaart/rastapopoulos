#ifndef _STRING_H_
#define _STRING_H_

#include <stddef.h>
#include <stdarg.h>

typedef void (*format_callback_t)(int, void*);

int strcmp(const char* s0, const char* s1);
void *memset(void *b, int c, size_t len);
void bzero(void *s, size_t n);
void *memcpy(void *restrict dst, const void *restrict src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlcpy(char * restrict dst, const char * restrict src, size_t size);
size_t strlcat(char * restrict dst, const char * restrict src, size_t size);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);

void formatv(format_callback_t callback, void* callback_params, const char* fmt, va_list args);
void format(format_callback_t callback, void* callback_params, const char* fmt,...);
void itox(char* str, unsigned n);
void itoa(char* str, unsigned n);

#endif //_STRING_H_

