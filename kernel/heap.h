#pragma once

#include <stddef.h>

void* heap_grow(size_t bytes);
void heap_shrink(size_t bytes);
void* heap_alloc(size_t size);
void* heap_alloc_aligned(size_t size, size_t alignment);
void heap_free(void* ptr);

