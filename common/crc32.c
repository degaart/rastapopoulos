#include "crc32.h"

uint32_t crc32_init() { return CRC32_INITIAL; }

uint32_t crc32_update(uint32_t state, const void *buffer, size_t size) {
  uint32_t r = state;
  const unsigned char *data = buffer;
  while (size--) {
    r ^= *data++;

    for (int i = 0; i < 8; i++) {
      uint32_t t = ~((r & 1) - 1);
      r = (r >> 1) ^ (0xEDB88320 & t);
    }
  }

  return r;
}

uint32_t crc32_finish(uint32_t state) { return ~state; }
