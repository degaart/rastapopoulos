#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

size_t strlen(const char* s);
size_t strlcpy(char* restrict dst, const char* restrict src, size_t dstsize);
size_t strlcat(char* restrict dst, const char* restrict src, size_t dstsize);
char* strdup(const char* str);
int strcmp(const char* s1, const char* s2);
void* memcpy(void* restrict dst, const void* restrict src, size_t len);
int memcmp(const void* ptr0, const void* ptr1, size_t len);
void* memset(void* dst, int ch, size_t len);
void itox(char* buffer, size_t size, unsigned value);
void itoa(char* buffer, size_t size, unsigned value);
void format(char* (*writefn)(const char*, void*, int), void* ctx, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));
int snprintf(char* buf, int count, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
int vsnprintf(char* buf, int count, const char* fmt, va_list va);
const char* basename(const char* filename);

#define UTF8_IS4(c)    (((c)&0xF8) == 0xF0)
#define UTF8_IS3(c)    (((c)&0xF0) == 0xE0)
#define UTF8_IS2(c)    (((c)&0xE0) == 0xC0)
#define UTF8_IS1(c)    (((c)&0x80) == 0x00)
#define UTF8_ISCONT(c) (((c)&0xC0) == 0x80)
