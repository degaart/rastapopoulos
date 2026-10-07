#include "vga.h"
#include "kernel.h"
#include <stddef.h>

#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA  0x3D5

static volatile uint16_t* vga_buffer = (volatile uint16_t*)0xb8000;
static struct VGAPoint cursor;

struct VGAPoint vga_get_cursor_pos(void)
{
    uint16_t pos;

    outb(VGA_CRTC_INDEX, 0x0E);
    pos = (uint16_t)inb(VGA_CRTC_DATA) << 8;

    outb(VGA_CRTC_INDEX, 0x0F);
    pos |= inb(VGA_CRTC_DATA);

    struct VGAPoint result = {.x = pos % 80, .y = pos / 80};
    return result;
}

void vga_set_cursor_pos(struct VGAPoint p)
{
    if (p.x >= VGA_WIDTH)
        p.x = VGA_WIDTH - 1;
    if (p.y >= VGA_HEIGHT)
        p.y = VGA_HEIGHT - 1;
    uint16_t pos = (uint16_t)p.y * 80 + p.x;

    outb(VGA_CRTC_INDEX, 0x0E);
    outb(VGA_CRTC_DATA, (uint8_t)(pos >> 8));

    outb(VGA_CRTC_INDEX, 0x0F);
    outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
}

void vga_init(void)
{
    cursor = vga_get_cursor_pos();
}

void vga_write(const char* text, int len, uint8_t fg, uint8_t bg)
{
    const uint16_t attr = (fg << 8) | bg;
    while ((len == -1 && *text) || len > 0) {
        char c = *text++;
        if (c == '\n') {
            cursor.x = 0;
            cursor.y++;
        } else {
            vga_buffer[cursor.y * VGA_WIDTH + cursor.x] = attr | (uint8_t)c;

            cursor.x++;

            if (cursor.x >= VGA_WIDTH) {
                cursor.x = 0;
                cursor.y++;
            }
        }

        if (cursor.y >= VGA_HEIGHT) {
            /* Scroll screen up by one row. */
            for (size_t i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); ++i) {
                vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
            }

            /* Clear the last row. */
            for (size_t i = VGA_WIDTH * (VGA_HEIGHT - 1);
                 i < VGA_WIDTH * VGA_HEIGHT; ++i) {
                vga_buffer[i] = attr | ' ';
            }

            cursor.y = VGA_HEIGHT - 1;
        }

        if (len != -1)
            len--;
    }
    vga_set_cursor_pos(cursor);
}

