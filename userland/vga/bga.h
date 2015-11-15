#ifndef _BGA_H_
#define _BGA_H_

/* Driver for the Bochs VGA driver */

#include <stdint.h>
#include <stdbool.h>

#define BGA_DISPI_IOPORT_INDEX (0x01CE)
#define BGA_DISPI_IOPORT_DATA (0x01CF)

#define BGA_DISPI_DISABLED (0x00)
#define BGA_DISPI_ENABLED (0x01)
#define BGA_DISPI_GETCAPS (0x02)
#define BGA_DISPI_LFB_ENABLED (0x40)
#define BGA_DISPI_NOCLEARMEM (0x80)

#define BGA_DISPI_ID0   (0xB0C0)
#define BGA_DISPI_ID1   (0xB0C1)
#define BGA_DISPI_ID2   (0xB0C2)
#define BGA_DISPI_ID3   (0xB0C3)
#define BGA_DISPI_ID4   (0xB0C4)
#define BGA_DISPI_ID5   (0xB0C5)

#define BGA_DISPI_BPP_4 (0x04)
#define BGA_DISPI_BPP_8 (0x08)
#define BGA_DISPI_BPP_15 (0x0F)
#define BGA_DISPI_BPP_16 (0x10)
#define BGA_DISPI_BPP_24 (0x18)
#define BGA_DISPI_BPP_32 (0x20) 

#define BGA_DISPI_BANK_ADDRESS           0xA0000
#define BGA_DISPI_BANK_SIZE_KB           64
#define BGA_DISPI_LFB_PHYSICAL_ADDRESS   0xE0000000

#define BGA_DISPI_INDEX_ID (0)
#define BGA_DISPI_INDEX_XRES (1)
#define BGA_DISPI_INDEX_YRES (2)
#define BGA_DISPI_INDEX_BPP (3)
#define BGA_DISPI_INDEX_ENABLE (4)
#define BGA_DISPI_INDEX_BANK (5)
#define BGA_DISPI_INDEX_VIRT_WIDTH (6)
#define BGA_DISPI_INDEX_VIRT_HEIGHT (7)
#define BGA_DISPI_INDEX_X_OFFSET (8)
#define BGA_DISPI_INDEX_Y_OFFSET (9)
#define BGA_DISPI_INDEX_VIDEO_MEMORY_64K (0xA)


void bga_write_reg(unsigned reg, unsigned val);
unsigned bga_read_reg(unsigned reg);
void bga_disable();
void bga_enable();
bool bga_set_res(uint32_t width, uint32_t height, uint32_t bpp);
bool bga_set_bank(unsigned index);

#endif //_BGA_H_

