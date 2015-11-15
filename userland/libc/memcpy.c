#include <string.h>
#include <stdint.h>

void *memcpy(void *restrict dst, const void *restrict src, size_t n) {
    const uint8_t* s = (const uint8_t*)src;
    uint8_t* d = (uint8_t*)dst;

    while(n--) {
        *d = *s;
        s++;
        d++;
    }
    return dst;
}
