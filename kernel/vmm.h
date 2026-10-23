#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VMM_PRESENT  (1 << 0)
#define VMM_WRITABLE (1 << 1)
#define VMM_USER     (1 << 2)
#define VMM_SHARED   (1 << 9)

bool vmm_init(void);
bool vmm_map(void* frame, void* virtual_address, uint32_t flags);
bool vmm_unmap(void* virtual_address);
bool vmm_remap(void* virtual_address, uint32_t flags);
bool vmm_frame(const void* virtual_address, void** frame_out);
bool vmm_is_valid_userptr(const void* ptr, size_t len, bool for_write);
bool vmm_is_valid_userstr(const char* str);
void* vmm_alloc(void* va, uint32_t flags);

