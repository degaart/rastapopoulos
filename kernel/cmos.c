#include "cmos.h"
#include "kernel.h"

#define CONTROL 0x70
#define DATA    0x71
#define NMI     0x80

uint8_t cmos_read(uint8_t reg)
{
    outb(CONTROL, reg);
    return inb(DATA);
}

void cmos_write(uint8_t reg, uint8_t val)
{
    outb(CONTROL, reg);
    outb(DATA, val);
}

