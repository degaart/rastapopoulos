#pragma once

#include <stdint.h>

struct kernel_heap_info {
    uint32_t heap_start;
    uint32_t heap_size;
};

void kmalloc_init(void* start);
void* kmalloc(unsigned size);
void* kmalloc_a(unsigned size, unsigned alignment);
void kfree(void* address);

/*
 * Used by vmm to map current heap into virtual address space
 */
void kernel_heap_info(struct kernel_heap_info* heap_info);

void heap_trace_start();
void heap_trace_stop();

void __kernel_heap_check(const char* file, int line);
#define kernel_heap_check() __kernel_heap_check(__FILE__, __LINE__)

