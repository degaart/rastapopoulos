#pragma once

#include <stdint.h>

#define CMOS_FLOPPY_DRIVES 0x10

uint8_t cmos_read(uint8_t reg);
void cmos_write(uint8_t reg, uint8_t val);

