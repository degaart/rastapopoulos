#pragma once

#include <stddef.h>

void early_malloc_init(void);
void* early_malloc(size_t size);
void* early_malloc_tail(void);

