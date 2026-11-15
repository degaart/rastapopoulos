#pragma once

#include "kernel.h"
#include <stddef.h>
#include <stdint.h>

bool heap_init(void) NODISCARD;
void* kmalloc(size_t size) NODISCARD;
void kfree(void* ptr);
void* kmemalign(size_t alignment, size_t size) NODISCARD;
#define knew(...) KNEW_SELECT(__VA_ARGS__, KNEW2, KNEW1)(__VA_ARGS__)

/* Select between two versions of knew() */
#define KNEW1(type)                    (type*)__knew(sizeof(type), 1)
#define KNEW2(type, count)             (type*)__knew(sizeof(type), count)
#define KNEW_SELECT(_1, _2, NAME, ...) NAME
void* __knew(size_t size, size_t count);

