#pragma once

#include <multiboot.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void pmm_init(const struct multiboot_mmap_entry* mmap, size_t mmap_len);
void* pmm_alloc(void);
/* Allocate a whole page below the exclusive physical address limit. */
void* pmm_alloc_below(uintptr_t limit);
void pmm_free(void* frame);
void pmm_dump(void);
bool pmm_reserve(void* frame);

/* Return amount of available memory in bytes */
size_t pmm_info(void);

