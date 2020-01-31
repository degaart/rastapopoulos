#pragma once

#include <stdbool.h>
#include "multiboot.h"

#define PAGE_SIZE 4096

void pmm_init(const struct multiboot_mmap_entry* memmap, int count);
void pmm_reserve_range(unsigned long page, size_t length);          /* length: bytes, must be a multiple of PAGE_SIZE */
void pmm_reserve(unsigned long page);
bool pmm_exists(unsigned long page);
bool pmm_reserved(unsigned long page);
void pmm_free_range(unsigned long page, size_t length);             /* length: bytes, multiple of PAGE_SIZE */
void pmm_free(unsigned long page);
void test_pmm();

