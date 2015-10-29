#include <rasta.h>
#include "vga.h"
#include "bga.h"
#include "morty.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

#define COLOR_RED 0xFF0000
#define COLOR_GREEN 0x00FF00
#define COLOR_BLUE 0x0000FF
#define COLOR_BLACK 0x000000
#define COLOR_WHITE 0xFFFFFF

static void run_vga(uint32_t port) {
    rs_trace("Initializing standard VGA textmode");
    rs_mmap((void*)VGA_BASE, VGA_BASE, MMAP_WRITABLE);  /* Todo: this should be mapped into userspace */
    vga_init();

    char buffer[512];
    struct Message_t msg;
    while(1) {
        msg.payload = buffer;
        msg.payload_size = sizeof(buffer);

        unsigned ret = rs_port_read(RS_PORT_VGA, &msg);
        if(ret) {
            rs_trace("rs_port_read() failed: need %u bytes, have %u bytes", ret, sizeof(buffer));
            return;
        }

        switch(msg.id) {
            case RS_MSG_VGA_WRITE_STRING:
                write_string_attr(buffer, msg.payload_size, COLOR_WHITE);
                break;
            default:
                rs_trace("Unhandled message id: 0x%X", msg.id);
                break;
        }
    }
}

/*
Displaying GFX (banked mode)
--------------
  What happens is that the total screen is devided in banks of 'VBE_DISPI_BANK_SIZE_KB' KiloByte in size.
  If you want to set a pixel you can calculate its bank by doing:

    offset = pixel_x + pixel_y * resolution_x;
    bank = offset / 64 Kb (rounded 1.9999 -> 1)

    bank_pixel_pos = offset - bank * 64Kb

  Now you can set the current bank and put the pixel at VBE_DISPI_BANK_ADDRESS + bank_pixel_pos
*/
static int current_bank = 0;
static void putpixel(unsigned x, unsigned y, unsigned color) {
    unsigned offset = (x*4) + ((y*4) * SCREEN_WIDTH);
    unsigned bank = offset / (BGA_DISPI_BANK_SIZE_KB*1024);
    unsigned bank_pixel_pos = offset - (bank * (BGA_DISPI_BANK_SIZE_KB*1024));
    uint32_t* pixel = (uint32_t*)(BGA_DISPI_BANK_ADDRESS + bank_pixel_pos);

    if(current_bank != bank) {
        // rs_trace("Switching to bank %u", bank);
        bga_write_reg(BGA_DISPI_INDEX_BANK, bank);
        current_bank = bank;
    }
    *pixel = color;
}

#define ZSoft_Manufacturer 10
#define PC_Paintbrush_Version 5
#define PCX_Uncompressed_Encoding 0
#define PCX_RunLength_Encoding 1

struct PCXheader {
    uint8_t Manufacturer;
    uint8_t Version;
    uint8_t Encoding;
    uint8_t BitsPerPixel;
    int16_t Xmin, Ymin, Xmax, Ymax;
    int16_t HDpi, VDpi;
    uint8_t Colormap[48];
    uint8_t Reserved;
    uint8_t NPlanes;
    int16_t BytesPerLine;
    int16_t PaletteInfo;
    int16_t HscreenSize;
    int16_t VscreenSize;
    uint8_t Filler[54];
} __attribute__((packed));

static void display_image(unsigned dest_x, unsigned dest_y) {
    uint8_t* ptr = (uint8_t*)MORTY;
    struct PCXheader* hdr = (struct PCXheader*)ptr;
    ptr += sizeof(struct PCXheader);

    int bpl = hdr->NPlanes * hdr->BytesPerLine;
    uint8_t* buf = (uint8_t*)0x800000;
    for(int y = dest_y; y <= dest_y + hdr->Ymax; y++) {
        uint8_t ch = '\0';
        unsigned count = 0;
        for(unsigned i = 0; i<bpl; i++) {
            if(!count) {
                ch = *(ptr++);
                if( (ch & 0xc0) == 0xc0) {
                    count = ch & 0x3f;
                    ch = *(ptr++);
                } else {
                    count = 1;
                }
            }
            buf[i] = ch;
            count--;
        }
        
        /* de-interlace planes */
        unsigned r, g, b;
        for(int x = dest_x; x <= dest_x + hdr->Xmax; x++) {
            if((x>=0 && x<SCREEN_WIDTH) && (y>=0 && y<SCREEN_HEIGHT)) {
                r = buf[x - dest_x];
                g = buf[x - dest_x + hdr->BytesPerLine];
                b = buf[x - dest_x + (hdr->BytesPerLine * 2)];
                unsigned col = (r << 16) | (g << 8) | b;
                putpixel(x, y, col);
            }
        }
    }
}

static void fillrect(unsigned x, unsigned y, unsigned width, unsigned height, unsigned color) {
    for(unsigned j = y; j < y + height; j++) {
        for(unsigned i = x; i < x + width; i++) {
            putpixel(i, j, color);
        }
    }
}

static void run_bga(uint32_t port) {
    rs_trace("Initializing BGA");
    if(!bga_set_res(SCREEN_WIDTH, SCREEN_HEIGHT, BGA_DISPI_BPP_32))
        return;

    for(uint32_t page = BGA_DISPI_BANK_ADDRESS; page <= BGA_DISPI_BANK_ADDRESS + (BGA_DISPI_BANK_SIZE_KB * 1024); page += 4096) {
        rs_mmap((void*)page, page, MMAP_WRITABLE);
    }

    rs_mmap((void*)0x800000, 0, MMAP_WRITABLE);

    struct PCXheader* hdr = (struct PCXheader*)MORTY;
    int img_width = (hdr->Xmax - hdr->Xmin) + 1;
    int img_height = (hdr->Ymax - hdr->Ymin) + 1;

    int x = 320, y = 240;
    int delta_x = 1, delta_y = 1;
    while(true) {
        if(delta_x > 0)
            fillrect(x, y, 1, img_height, COLOR_BLACK);
        else if(delta_x < 0)
            fillrect(x + img_width, y, 1, img_height + 1, COLOR_BLACK);

        if(delta_y > 0)
            fillrect(x, y, img_width, 1, COLOR_BLACK);
        else if(delta_y < 0)
            fillrect(x, y + img_height, img_width + 1, 1, COLOR_BLACK);

        x += delta_x;
        y += delta_y;

        if(x < 0) {
            x = 0;
            delta_x = -delta_x;
        } else if(x + img_width > SCREEN_WIDTH) {
            x = SCREEN_WIDTH - img_width;
            delta_x = -delta_x;
        }

        if(y < 0) {
            y = 0;
            delta_y = -delta_y;
        } else if(y + img_height > SCREEN_HEIGHT) {
            y = SCREEN_HEIGHT - img_height;
            delta_y = -delta_y;
        }
        display_image(x, y);

        //rs_yield();
    }

}

int main() {
    rs_trace("vgadrv started");

    uint32_t port = rs_port_open(RS_PORT_VGA);
    if(port == 0) {
        rs_trace("Failed to open port %u", RS_PORT_VGA);
        return 1;
    }

    unsigned enabled = bga_read_reg(BGA_DISPI_INDEX_ENABLE);
    rs_trace("BGA enabled: 0x%X", enabled);

    unsigned result = bga_read_reg(BGA_DISPI_INDEX_ID);
    rs_trace("dispid: 0x%X", result);
    if(result >= BGA_DISPI_ID0 && result <= BGA_DISPI_ID5) {
        run_bga(port);
    } else {
        run_vga(port);
    }

    rs_port_close(port);
    return 0;
}



