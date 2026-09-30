#include "crc32.h"

uint32_t crc32_update(uint32_t crc, const void* data, size_t size)
{
    const uint8_t* p = (const uint8_t*)data;

    while (size--) {
        crc ^= *p++;

        for (unsigned int i = 0; i < 8; ++i) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }

    return crc;
}

uint32_t crc32_finish(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}

