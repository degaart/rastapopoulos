#include <string.h>
#include <stdint.h>

void *memset(void *b, int c, size_t len) {
    uint8_t* p = (uint8_t*)b;
    while(len--) {
        *p = c;
        p++;
    }
    return b;
}
