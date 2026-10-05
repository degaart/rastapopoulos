#include "kernel.h"
#include "vga.h"
#include <string.h>

void kmain()
{
    vga_init();
    const char* msg[] = {"Line 0\n"
                         "Line 1\n"
                         "Line 2\n"
                         "Line 3\n"
                         "Line 4\n"
                         "Line 5\n"
                         "Line 6\n"
                         "Line 7\n"
                         "Line 8\n"
                         "Line 9\n"
                         "Line 10\n"
                         "Line 11\n"
                         "Line 12\n"
                         "Line 13\n"
                         "Line 14\n"};

    uint8_t bg = VGA_COLOR_BLACK;
    uint8_t fg = VGA_COLOR_LIGHT_CYAN;
    for (int i = 0; i < sizeof(msg) / sizeof(msg[0]); i++) {
        vga_write(msg[i], fg, bg);
        const char* ptr = msg[i];
    }
    while (1)
        ;
}

