#include <string.h>
#include <stdint.h>

int memcmp(const void *s1, const void *s2, size_t n) {
    if(!n)
        return 0;

    uint8_t* ptr0 = (uint8_t*)s1;
    uint8_t* ptr1 = (uint8_t*)s2;

    while(n--) {
        if(*ptr0 != *ptr1)
            return *ptr1 - *ptr0;
        ptr0++;
        ptr1++;
    }
    return 0;
}

