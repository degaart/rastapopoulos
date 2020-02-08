#pragma once

/*
 * Gotchas:
 *  - must copy kernel mappings before switching to new pagedir
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


/* Inclusive */
#define KERNEL_AREA_START    0x00100000
#define KERNEL_AREA_END      0x003FFFFF
#define USER_AREA_START      0x00400000
#define USER_AREA_END        0xFF7FFFFF

/* flags */
#define VMM_PAGE_WRITABLE       0x1
#define VMM_PAGE_USER           0x2

void vmm_init();
bool vmm_initialized();
void vmm_map(void* page, unsigned long frame, unsigned flags);
void vmm_unmap(void* page);
void vmm_remap(void* page, unsigned flags);
uint32_t vmm_get_frame(void* page);
uint32_t* vmm_create_pagedir();
uint32_t* vmm_create_pagetable();

void test_vmm();

