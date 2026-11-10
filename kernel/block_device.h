#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct BlockDevice
{
    bool (*check_media)(struct BlockDevice*, unsigned* generation);
    bool (*read)(struct BlockDevice*, uint64_t lba, char* buffer,
                 size_t buffer_len);
    bool (*write)(struct BlockDevice*, uint64_t lba, const char* buffer,
                  size_t buffer_len);
};

