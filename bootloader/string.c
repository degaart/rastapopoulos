#include "string.h"
#include <stdint.h>

void* memset(void* dest, int ch, size_t count)
{
    uint8_t* ptr = dest;
    while(count--)
    {
        *(ptr++) = ch;
    }
}


