#pragma once

#include <stddef.h>
#include <stdbool.h>

extern bool early_kmalloc_enabled;
void* early_kmalloc(size_t size);
void* early_kmalloc_aligned(size_t size, size_t alignment);
void* early_kmalloc_get_heap(void);


