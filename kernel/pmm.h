#pragma once

#include <stdbool.h>
#include "multiboot.h"

#define PAGE_SIZE 4096
#define INVALID_PAGE 0xFFFFFFFF

void pmm_init(const struct multiboot_mmap_entry* memmap, int count);
bool pmm_initialized();
void pmm_reset();
void pmm_reserve_range(unsigned long page, size_t length);          /* length: bytes, must be a multiple of PAGE_SIZE */
void pmm_reserve(unsigned long page);
bool pmm_exists(unsigned long page);
bool pmm_reserved(unsigned long page);
void pmm_free_range(unsigned long page, size_t length);             /* length: bytes, multiple of PAGE_SIZE */
void pmm_free(unsigned long page);
unsigned long pmm_find(size_t length);                              /* length: bytes, multiple of page size. Returns INVALID_PAGE if not found */
void test_pmm();

