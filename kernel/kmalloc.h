#pragma once

#include <stdint.h>
#include <stddef.h>

void kmalloc_init(void* kernel_end);
void* kmalloc(size_t size);
void kfree(void*);
void test_kmalloc();


