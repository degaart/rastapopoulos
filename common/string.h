#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

size_t strlen(const char* s);
size_t strlcpy(char* restrict dst, const char* restrict src, size_t dstsize);
size_t strlcat(char* restrict dst, const char* restrict src, size_t dstsize);
int strcmp(const char* s1, const char* s2);
void* memcpy(void* restrict dst, const void* restrict src, size_t len);
int memcmp(const void* ptr0, const void* ptr1, size_t len);
void* memset(void* dst, int ch, size_t len);
void itox(char* buffer, size_t size, unsigned value);
void itoa(char* buffer, size_t size, unsigned value);
void format(bool (*writefn)(char,void*), void* ctx, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
void formatv(bool (*writefn)(char,void*), void* ctx, const char* fmt, va_list args);
int snprintf(char* restrict str, size_t size, const char* restrict fmt, ...) __attribute__((format(printf, 3, 4)));
int vsnprintf(char* restrict str, size_t size, const char* restrict fmt, va_list args);
const char* basename(const char* filename);
