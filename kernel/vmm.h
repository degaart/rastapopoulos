#pragma once

#include <stdbool.h>
#include <stdint.h>

#define VMM_PAGE_SIZE 4096
#define VMM_WRITABLE  0x002
#define VMM_USER      0x004

#define VMM_RECURSIVE_BASE 0xffc00000

/*
 * All calls require interrupts disabled and exclusive access to this address
 * space
 */

bool vmm_init(void);
bool vmm_map(void* frame, void* virtual_address, uint32_t flags);
bool vmm_unmap(void* virtual_address);
bool vmm_remap(void* virtual_address, uint32_t flags);
bool vmm_frame(const void* virtual_address, void** frame_out);

