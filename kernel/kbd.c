#include "pic.h"
#include <io.h>
#include <debug.h>

#define REG_DATA        0x60
#define REG_CONTROL     0x64
#define EXTENDED        0xE0

static void irq_handler()
{
    uint8_t scancode = inb(REG_DATA);
    if(scancode & 0x80) {
        /* Key released */
        scancode = scancode & ~0x80;
        TRACE("kbd: released 0x%X", scancode);
    } else {
        /* Key pressed (repeating) */
        TRACE("kbd: pressed  0x%X", scancode);
    }
}

void kbd_init()
{
    pic_set_irq_handler(1, irq_handler);
}

