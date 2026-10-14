#pragma once

#include <stddef.h>

struct Bitset;

void early_malloc_init(const struct Bitset* bootstrap_memmap);
void* early_malloc(size_t size);
void* early_malloc_tail(void);
void early_free(void*);

