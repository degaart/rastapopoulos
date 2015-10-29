#include "bga.h"
#include <rasta.h>

void bga_write_reg(unsigned reg, unsigned val) {
    rs_outw(BGA_DISPI_IOPORT_INDEX, reg);
    rs_outw(BGA_DISPI_IOPORT_DATA, val);
}

unsigned bga_read_reg(unsigned reg) {
    rs_outw(BGA_DISPI_IOPORT_INDEX, reg);
    return rs_inw(BGA_DISPI_IOPORT_DATA);
}

void bga_disable() {
    bga_write_reg(BGA_DISPI_INDEX_ENABLE, BGA_DISPI_DISABLED);
}

void bga_enable() {
    bga_write_reg(BGA_DISPI_INDEX_ENABLE, BGA_DISPI_ENABLED);
}

bool bga_set_res(uint32_t width, uint32_t height, uint32_t bpp) {
    if(width % 8) {
        rs_trace("Invalid width: %u", width);
        return false;
    } else if(height % 8) {
        rs_trace("Invalid height: %u", height);
        return false;
    }

    switch(bpp) {
        case BGA_DISPI_BPP_4:
        case BGA_DISPI_BPP_8:
        case BGA_DISPI_BPP_15:
        case BGA_DISPI_BPP_16:
        case BGA_DISPI_BPP_24:
        case BGA_DISPI_BPP_32:
            break;
        default:
            rs_trace("Invalid bpp: %u", bpp);
            return false;
    }

    bga_disable();
    bga_write_reg(BGA_DISPI_INDEX_XRES, width);
    bga_write_reg(BGA_DISPI_INDEX_YRES, height);
    bga_write_reg(BGA_DISPI_INDEX_BPP, bpp);

    /* Check if really changed */
    if(
        bga_read_reg(BGA_DISPI_INDEX_XRES) != width || 
        bga_read_reg(BGA_DISPI_INDEX_YRES) != height ||
        bga_read_reg(BGA_DISPI_INDEX_BPP) != bpp
    ) {
        rs_trace("Failed to set mode to %ux%ux%u", width, height, bpp);
        return false;
    }

    /* All ok, reenable BGA extensions */
    bga_enable();
 
    return true;
}

bool bga_set_bank(unsigned index) {
    bga_write_reg(BGA_DISPI_INDEX_BANK, index);
    return bga_read_reg(BGA_DISPI_INDEX_BANK) == index;
}


