#include "string.h"

void* memset(void* dest, int ch, size_t count)
{
    unsigned char* out = dest;

    while (count-- != 0)
        *out++ = (unsigned char)ch;

    return dest;
}

int memcmp(const void* lhs, const void* rhs, size_t count)
{
    const unsigned char* a = lhs;
    const unsigned char* b = rhs;

    while (count-- != 0) {
        if (*a != *b)
            return (*a > *b) ? 1 : -1;

        ++a;
        ++b;
    }

    return 0;
}

void* memcpy(void* restrict dest, const void* restrict src, size_t count)
{
    unsigned char* out = dest;
    const unsigned char* in = src;

    while (count-- != 0)
        *out++ = *in++;

    return dest;
}

void* memchr(const void* ptr, int ch, size_t count)
{
    const unsigned char* p = ptr;
    unsigned char needle = (unsigned char)ch;

    while (count-- != 0) {
        if (*p == needle)
            return (void*)p;

        ++p;
    }

    return NULL;
}

void* memmove(void* dest, const void* src, size_t count)
{
    unsigned char* out = dest;
    const unsigned char* in = src;

    if (out == in || count == 0)
        return dest;

    if (out < in) {
        while (count-- != 0)
            *out++ = *in++;
    } else {
        out += count;
        in += count;

        while (count-- != 0)
            *--out = *--in;
    }

    return dest;
}

size_t strlen(const char* str)
{
    const char* p = str;

    while (*p != '\0')
        ++p;

    return (size_t)(p - str);
}

size_t strnlen_s(const char* str, size_t strsz)
{
    size_t length = 0;

    if (str == NULL)
        return 0;

    while (length < strsz && str[length] != '\0')
        ++length;

    return length;
}

int strcmp(const char* lhs, const char* rhs)
{
    while (*lhs == *rhs) {
        if (*lhs == '\0')
            return 0;

        ++lhs;
        ++rhs;
    }

    return ((unsigned char)*lhs > (unsigned char)*rhs) ? 1 : -1;
}

int strncmp(const char* lhs, const char* rhs, size_t count)
{
    while (count-- != 0) {
        if (*lhs != *rhs)
            return ((unsigned char)*lhs > (unsigned char)*rhs) ? 1 : -1;

        if (*lhs == '\0')
            return 0;

        ++lhs;
        ++rhs;
    }

    return 0;
}

char* strchr(const char* str, int ch)
{
    char needle = (char)ch;

    do {
        if (*str == needle)
            return (char*)str;
    } while (*str++ != '\0');

    return NULL;
}

char* strrchr(const char* str, int ch)
{
    const char* result = NULL;
    char needle = (char)ch;

    do {
        if (*str == needle)
            result = str;
    } while (*str++ != '\0');

    return (char*)result;
}

char* strstr(const char* str, const char* substr)
{
    if (*substr == '\0')
        return (char*)str;

    for (; *str != '\0'; ++str) {
        const char* haystack = str;
        const char* needle = substr;

        while (*needle != '\0' && *haystack == *needle) {
            ++haystack;
            ++needle;
        }

        if (*needle == '\0')
            return (char*)str;
    }

    return NULL;
}

int strcpy_s(char* restrict dest, size_t destsz, const char* restrict src)
{
    size_t length;

    if (dest == NULL || destsz == 0 || src == NULL)
        return 1;

    length = strnlen_s(src, destsz);
    if (length == destsz) {
        dest[0] = '\0';
        return 1;
    }

    memcpy(dest, src, length + 1);
    return 0;
}

int strncpy_s(char* restrict dest, size_t destsz, const char* restrict src,
              size_t count)
{
    size_t length;

    if (dest == NULL || destsz == 0 || src == NULL)
        return 1;

    length = strnlen_s(src, count);

    if (length == count) {
        if (count >= destsz) {
            dest[0] = '\0';
            return 1;
        }

        memcpy(dest, src, count);
        dest[count] = '\0';
        return 0;
    }

    if (length >= destsz) {
        dest[0] = '\0';
        return 1;
    }

    memcpy(dest, src, length + 1);
    return 0;
}

int strcat_s(char* restrict dest, size_t destsz, const char* restrict src)
{
    size_t dest_length;
    size_t src_length;

    if (dest == NULL || destsz == 0 || src == NULL)
        return 1;

    dest_length = strnlen_s(dest, destsz);
    if (dest_length == destsz) {
        dest[0] = '\0';
        return 1;
    }

    src_length = strnlen_s(src, destsz - dest_length);
    if (src_length == destsz - dest_length) {
        dest[0] = '\0';
        return 1;
    }

    memcpy(dest + dest_length, src, src_length + 1);
    return 0;
}

#ifndef RASTA_BOOTLOADER
size_t uint64_to_string(uint64_t value, char* buffer, size_t buffer_len)
{
    if (buffer == NULL || buffer_len == 0) {
        return 0;
    }

    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    char temporary[20];
    char* end = temporary + sizeof temporary;
    char* p = end;

    const uint32_t base = 1000000000u;
    while (hi != 0) {
        uint32_t quotient_hi = hi / base;
        uint32_t remainder = hi % base;
        asm volatile("div{l %[base]| %[base]}"
                     : "+a"(lo), "+d"(remainder)
                     : [base] "r"(base)
                     : "cc");

        hi = quotient_hi;
        for (unsigned int i = 0; i < 9; ++i) {
            uint32_t quotient = remainder / 10u;
            *--p = (char)('0' + remainder - quotient * 10u);
            remainder = quotient;
        }
    }

    do {
        uint32_t quotient = lo / 10u;
        *--p = (char)('0' + lo - quotient * 10u);
        lo = quotient;
    } while (lo != 0);

    size_t length = (size_t)(end - p);
    if (buffer_len <= length) {
        return 0;
    }

    for (size_t i = 0; i < length; ++i) {
        buffer[i] = p[i];
    }

    buffer[length] = '\0';
    return length;
}
#endif

