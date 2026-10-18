#include "vga.h"

static volatile uint16_t* vga_buffer = (volatile uint16_t*)VGA_BASE;

void vga_write_at(int x, int y, char ch, uint8_t fg, uint8_t bg)
{
    uint16_t attr = (bg << 4) | fg;
    vga_buffer[y * VGA_WIDTH + x] = (attr << 8) | (uint8_t)ch;
}

