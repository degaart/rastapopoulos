#pragma once

#include <stdint.h>

void __attribute__((cdecl)) hmemset(uint32_t vaddr, int ch, uint32_t size);

