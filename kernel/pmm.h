#pragma once

#include "multiboot.h"
#include <stdbool.h>

#define PAGE_SIZE 4096

void pmm_init(const struct multiboot_info* multiboot_info);
bool pmm_initialized();
void pmm_reserve(uint32_t page);
bool pmm_exists(uint32_t page);
bool pmm_reserved(uint32_t page);
void pmm_free(uint32_t page);

#define PMM_INVALID_PAGE 0xFFFFFFFF
uint32_t pmm_alloc(); /* Returns PMM_INVALID_PAGE on error */



