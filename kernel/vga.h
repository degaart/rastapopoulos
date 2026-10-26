#pragma once

#include "kernel.h"
#include <stddef.h>
#include <stdint.h>

#define VGA_PHYSICAL_BASE       0xb8000
#define VGA_PHYSICAL_START      0xa0000
#define VGA_PHYSICAL_END        0xbffff
#define VGA_BASE                (VGA_PHYSICAL_BASE + KERNEL_OFFSET)
#define VGA_START               (VGA_PHYSICAL_START + KERNEL_OFFSET)
#define VGA_END                 (VGA_PHYSICAL_END + KERNEL_OFFSET)
#define VGA_COLOR_BLACK         0x0
#define VGA_COLOR_BLUE          0x1
#define VGA_COLOR_GREEN         0x2
#define VGA_COLOR_CYAN          0x3
#define VGA_COLOR_RED           0x4
#define VGA_COLOR_MAGENTA       0x5
#define VGA_COLOR_BROWN         0x6
#define VGA_COLOR_LIGHT_GREY    0x7
#define VGA_COLOR_DARK_GREY     0x8
#define VGA_COLOR_LIGHT_BLUE    0x9
#define VGA_COLOR_LIGHT_GREEN   0xA
#define VGA_COLOR_LIGHT_CYAN    0xB
#define VGA_COLOR_LIGHT_RED     0xC
#define VGA_COLOR_LIGHT_MAGENTA 0xD
#define VGA_COLOR_YELLOW        0xE
#define VGA_COLOR_WHITE         0xF

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

#define VGA_MODE_X_WIDTH  320
#define VGA_MODE_X_HEIGHT 240

struct VGAPoint
{
    uint8_t x;
    uint8_t y;
};

/* Opaque structure that represents a bitmap, converted to the
 * pixel format of the primary buffer
 */
struct Bitmap;

struct VGAPoint vga_get_cursor_pos(void);
void vga_set_cursor_pos(struct VGAPoint p);
void vga_init(void);
void vga_write(const char* text, int len, uint8_t fg, uint8_t bg);
void vga_write_at(int x, int y, char ch, uint8_t fg, uint8_t bg);
bool vga_lock_screen();
bool vga_unlock_screen();
void vga_putpixel(unsigned x, unsigned y, uint8_t color);
void vga_text_mode(void);
void vga_mode_x(void);

/*
 * Drawing uses the backbuffer
 * Rectangle endpoints are inclusive and may be reversed
 * Pixels outside the screen are clipped.
 */

/*
 * Swap the backbuffer into the primary buffer
 * Does not wait for vertical retrace
 */
void vga_swap(void);

/*
 * Clear the entire screen
 */
void vga_clear(uint8_t color);

/*
 * Draw a filled rectangle at the specified coordinates
 */
void vga_fillrect(int x1, int y1, int x2, int y2, uint8_t color);

/*
 * Draw a rectangle outline at the specified coordinates
 */
void vga_rect(int x1, int y1, int x2, int y2, uint8_t color);

/*
 * Draw a line
 */
void vga_drawline(int x1, int y1, int x2, int y2, uint8_t color);

/*
 * Draw an oval contained into the specified rectangle coordinates
 */
void vga_oval(int x1, int y1, int x2, int y2, uint8_t color);

/*
 * Draw a rounded rectangle
 */
void vga_roundrect(int x1, int y1, int x2, int y2, int radius, uint8_t color);

/*
 * Blit a bitmap into the specified coordinates. NULL selects the screen
 * backbuffer for either bitmap. Overlapping copies are supported.
 */
void vga_blit(struct Bitmap* src, int x1, int y1, struct Bitmap* dst, int x2,
              int y2, int width, int height);

/*
 * Load a raw array of pixel colors (8 bit per color)
 * Then convert it to the pixel format of the primary buffer,
 * for optimized blit operations
 */
struct Bitmap* vga_load_bitmap(const void* buffer, size_t len, int width,
                               int height);

/*
 * Destroys a bitmap (free memory)
 */
void vga_destroy_bitmap(struct Bitmap* bitmap);

/*
 * Set the color at palette index to the corresponding RGB value
 */
void vga_set_palette(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);

