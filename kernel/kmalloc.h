#pragma once

#include <stdint.h>
#include <stddef.h>

void kmalloc_init(const void* kernel_end);
void* kmalloc(size_t size);
void* kmalloc_aligned(size_t size, size_t alignment);
void* kmalloc_brk();
void kfree(void*);
void test_kmalloc();


