#include <string.h>

void itox(char* str, unsigned n) {
    char* out = str;
    unsigned nibble = 8;

    while(nibble) {
        unsigned shift = (nibble - 1) * 4;
        int digit = (n >> shift) & 0x0F;
        if(digit < 10)
            *(out++) = (char)('0' + digit);
        else
            *(out++) = (char)('A' + digit - 10);
       
        nibble--;
    }
    *out = '\0';
}

