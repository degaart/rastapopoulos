#pragma once

#define COLOR_BLACK         0x00
#define COLOR_BLUE          0x01
#define COLOR_GREEN         0x02
#define COLOR_CYAN          0x03
#define COLOR_RED           0x04
#define COLOR_MAGENTA       0x05
#define COLOR_BROWN         0x06
#define COLOR_LIGHTGRAY     0x07
#define COLOR_DARKGRAY      0x08
#define COLOR_LIGHBLUE      0x09
#define COLOR_LIGHTGREEN    0x0A
#define COLOR_LIGHCYAN      0x0B
#define COLOR_LIGHRED       0x0C
#define COLOR_LIGHTMAGENTA  0x0D
#define COLOR_YELLOW        0x0E
#define COLOR_WHITE         0x0F

#define VGA_BASE            0xB8000
#define VGA_WIDTH           80
#define VGA_HEIGHT          25

void vga_init();
void vga_cursor_pos(unsigned* x, unsigned* y);
void vga_set_cursor_pos(unsigned x, unsigned y);
void vga_put_char(unsigned x, unsigned y, unsigned ch, unsigned attr);
void vga_write_char(unsigned ch, unsigned attr);
void vga_write_string(const char* s, unsigned attr);
void vga_scroll();

