#pragma once

#include <stdint.h>

struct kernel_heap_info {
    uint32_t heap_start;
    uint32_t heap_size;
};

void* kmalloc(unsigned size);
void* kmalloc_a(unsigned size, unsigned alignment);
void kernel_heap_info(struct kernel_heap_info* heap_info);




