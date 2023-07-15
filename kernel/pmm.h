#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <multiboot.h>

#define INVALID_FRAME 0xFFFFFFFF

uint32_t pmm_alloc();
bool pmm_get(uint32_t phys_addr);
void pmm_set(uint32_t phys_addr);
void pmm_clear(uint32_t phys_addr);
void pmm_init(const struct multiboot_mmap_entry* entries, size_t count);

