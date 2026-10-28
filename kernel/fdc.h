#pragma once

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
/* Kernel buffers; byte lengths must be positive multiples of 512.
 * Failures may leave preceding sectors transferred. */
bool fdc_read(int drive, unsigned lba, char* buffer, size_t buffer_len);
bool fdc_write(int drive, unsigned lba, const char* buffer, size_t buffer_len);

#ifdef UNIT_TESTS
unsigned fdc_test_attempt_count(void);
#endif
