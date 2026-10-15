#pragma once

#include "kernel.h"
#include <stddef.h>
#include <stdint.h>

bool heap_init(void) NODISCARD;
void* kmalloc(size_t size) NODISCARD;
void kfree(void* ptr);
void* kmemalign(size_t alignment, size_t size) NODISCARD;

