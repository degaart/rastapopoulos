#pragma once

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

typedef void (*format_callback_t)(int, void*);

void itoa(char* str, unsigned n);
void itox(char* str, unsigned n);
uint32_t xtoa(const char* str);
int formatv(format_callback_t callback, 
            void* callback_params,
            const char* fmt,
            va_list args);
int format(format_callback_t callback, 
           void* callback_params,
           const char* fmt,
           ...);
void memset(void* buffer, int ch, uint32_t size);
void bzero(void* buffer, uint32_t size);
void memcpy(void* dest, const void* source, size_t size);
int memcmp(const void* p0, const void* p1, size_t size);
void strlcpy(char* dst, const char* src, unsigned siz);
size_t strlen(const char* str);
void strlcat(char* dst, const char* src, unsigned siz);
int strcmp(const char* s0, const char* s1);
int vsnprintf(char* buffer, size_t size, const char* fmt, va_list args);
int snprintf(char* buffer, size_t size, const char* fmt, ...);
int vsncatf(char* buffer, size_t size, const char* fmt, va_list args);
int sncatf(char* buffer, size_t size, const char* fmt, ...);
const char* basename(const char* filename);

/*
 * RASTAPOPOULOS_KERNEL isn't defined when compiling common.a
 * So we must define this function as a static inline
 */
#ifdef RASTAPOPOULOS_KERNEL
#include "../kernel/kmalloc.h"
static char* strdup(const char* str)
{
    size_t size = strlen(str);
    char* buffer = kmalloc(size + 1);
    memcpy(buffer, str, size + 1);
    return buffer;
}
#endif

