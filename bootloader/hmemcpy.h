#pragma once

#include <stddef.h>
#include <stdint.h>

void __attribute__((cdecl)) hmemcpy(uint32_t dst, const void* src, size_t len);

