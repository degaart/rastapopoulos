#pragma once

#include "block_device.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum FloppyType
{
    FDC_NONE,
    FDC_360,  /* 5.25 360K */
    FDC_1200, /* 5.25 1.2M */
    FDC_720,  /* 3.5 720K */
    FDC_1440, /* 3.5 1.44M */
    FDC_2880, /* 3.5 2.88M */
};

bool fdc_init(void);

/* Checks hardware without spin-up; leaves generation unchanged on failure. */
bool fdc_check_media(struct BlockDevice* device, unsigned* generation);

/*
 * Kernel buffers
 * Supports multiple sectors
 * Byte lengths must be positive multiples of 512
 * Failures may leave preceding sectors transferred
 */
bool fdc_read(struct BlockDevice* device, uint64_t lba, char* buffer,
              size_t buffer_len);
bool fdc_write(struct BlockDevice* device, uint64_t lba, const char* buffer,
               size_t buffer_len);

/* Get struct BlockDevice for drive number */
struct BlockDevice* fdc_drive(int drive);

#ifdef UNIT_TESTS
unsigned fdc_test_attempt_count(void);
#endif
