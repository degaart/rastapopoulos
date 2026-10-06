#include "kernel.h"
#include "vga.h"
#include <string.h>

void kmain()
{
    vga_init();
    vga_write("RastapopoulOS kernel running\n", VGA_COLOR_LIGHT_CYAN,
              VGA_COLOR_BLACK);
    while (1)
        ;
}

