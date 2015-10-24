#ifndef _INITRD_HEADER_H_
#define _INITRD_HEADER_H_

#include <stdint.h>

struct InitrdHeader_t {
    uint32_t size;
    char name[8+3+1+1];          /* null-terminated */
    uint8_t last;
} __attribute__((packed));

#endif // _INITRD_HEADER_H_
