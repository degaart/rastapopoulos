/**
 * Virtual memory manager
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VMM_PAGE_PRESENT        0x1
#define VMM_PAGE_WRITABLE       0x2
#define VMM_PAGE_USER           0x4

void vmm_init();
void vmm_map(uint32_t va, uint32_t pa, uint32_t flags);
void vmm_unmap(uint32_t va);
void vmm_remap(uint32_t va, uint32_t flags);
void vmm_flush_tlb(uint32_t va);
bool vmm_paging_enabled();
void invlpg(uint32_t va);


