#pragma once

#include <stddef.h>
#include <stdint.h>

bool heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
void* kmemalign(size_t alignment, size_t size);

