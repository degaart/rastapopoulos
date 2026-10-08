#pragma once

#include <multiboot.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PMM_FRAME_SIZE 4096

void pmm_init(const struct multiboot_mmap_entry* mmap, size_t mmap_len);
void* pmm_alloc(void);
void pmm_free(void* frame);
void pmm_dump(void);

/* Return amount of available memory in bytes */
size_t pmm_info(void);

