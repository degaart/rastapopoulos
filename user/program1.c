#include "user.h"
#include <debug.h>
#include <string.h>
#include <util.h>
#include <vga.h>

__attribute__((section(".entry"))) void _start()
{
    uint16_t* vga_base = (uint16_t*)VGA_BASE;
    unsigned counter = 0;
    char buffer[128];
    while(1) {
        uint32_t ticks = get_ticks();
        snprintf(buffer, sizeof(buffer), "%02d 0x%08lX 0x%08X", 0, ticks,
                 counter);
        uint16_t* d = vga_base + (0 * VGA_WIDTH);
        for(char* s = buffer; *s; s++, d++) {
            *d = *s | (uint16_t)(0x1F << 8);
        }
        counter++;
    }
    halt();
}
