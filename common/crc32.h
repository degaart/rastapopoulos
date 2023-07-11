#pragma once

#include <stddef.h>
#include <stdint.h>

#define CRC32_INITIAL 0xFFFFFFFF

uint32_t crc32_init();
uint32_t crc32_update(uint32_t state, const void* buffer, size_t size);
uint32_t crc32_finish(uint32_t state);
