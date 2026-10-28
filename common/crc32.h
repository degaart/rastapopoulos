#pragma once

#include <stddef.h>
#include <stdint.h>

#define CRC32_INIT 0xFFFFFFFFu

uint32_t crc32_update(uint32_t crc, const void* data, size_t size);
uint32_t crc32_finish(uint32_t crc);

