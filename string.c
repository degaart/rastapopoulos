#include "string.h"

size_t strlen(const char* str)
{
    size_t len = 0;
    while (*(str++))
        len++;
    return len;
}

void itox(char* buffer, size_t size, unsigned value)
{
    char tmp[12];
    char* p = tmp;
    while(value) {
        int digit = value % 16;
        *(p++) = digit + (digit < 10 ? '0' : 'A' - 10);
        value /= 16;
    }

    for(--p; p>=tmp && size > 1; size--) {
        *(buffer++) = *(p--);
    }
    *buffer = '\0';
}

void itoa(char* buffer, size_t size, unsigned value)
{
    char tmp[9];
    char* p = tmp;
    while(value) {
        *(p++) = (value % 10) + '0';
        value /= 10;
    }

    for(--p; p>=tmp && size > 1; size--) {
        *(buffer++) = *(p--);
    }
    *buffer = '\0';
}


