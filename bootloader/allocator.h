#pragma once

#include <stddef.h>

/* The arena must be in DS and no larger than 65535 bytes. */
void heapInit(void* arena, size_t size);
void* malloc(size_t size);
void free(void* ptr);

