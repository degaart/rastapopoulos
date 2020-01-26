#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "io.h"
#include "halt.h"
#include "debug.h"
#include "reboot.h"
#include "string.h"
#include "registers.h"
#include "gdt.h"

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static int vga_x = 0;
static int vga_y = 0;
static uint16_t* const vga_buffer = (uint16_t*)0xB8000;

#if 0
static void write_char(int ch, int fg, int bg)
{
    vga_buffer[(vga_y * VGA_WIDTH) + vga_x] = (ch & 0xFF) | (((fg & 0xF) | ((bg & 0xF) << 4)) << 8);
    vga_x++;
    if(vga_x >= VGA_WIDTH) {
        vga_y++;
        vga_x = 0;

        /* TODO: Scroll */
    }
}
#endif

static void write_char(int ch, int fg, int bg)
{
    outb(IOPORT_DEBUG, ch);
}

static void write_string(const char* str)
{
    int fg = VGA_COLOR_LIGHT_GREY;
    int bg = VGA_COLOR_BLACK;
    while(*str) {
        write_char(*str, fg, bg);
        str++;
    }
}

static void debug_write_char(int ch, void* unused)
{
    outb(IOPORT_DEBUG, ch);
}

void kmain()
{
    gdt_init();
    trace("All done, rebooting");
    reboot();
}


