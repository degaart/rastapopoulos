/*
 * Simple block cache using CLOCK replacement policy
 * Media change detection use generation number from the block device driver
 * Fixed block size (512 bytes)
 * Fixed cache size (128 entries)
 * Write-through
 */
#pragma once

#include "refcount.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct BlockCacheEntry
{
    int drive;
    unsigned generation;
    uint64_t lba;
    unsigned flags;
    REFCOUNT();
    char data[512];
};

bool block_cache_init(void);
bool block_cache_read(int drive, uint64_t lba, char* buffer,
                      size_t buffer_len);
bool block_cache_write(int drive, uint64_t lba, const char* buffer,
                       size_t buffer_len);

