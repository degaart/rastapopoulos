#include "user.h"
#include <debug.h>
#include <string.h>
#include <util.h>
#include <vga.h>

#if 0
struct kbd_event {
    unsigned scancode;
    unsigned modifiers;
    int ch;
    char unicode_ch[4];
};

static size_t readline(char* buffer, size_t size)
{
    size_t result = 0;
    while(size > 1) {
        /* should never fail (blocks when no events are queued) */
        kbd_read(&kbd_event);
        if(kbd_event.ch) {
            /* write char at current cursor pos and advance cursor */
            vga_putchar(kbd_event.ch);
            if(kbd_event.ch == '\n') {
                break;
            } else {
                *buffer = kbd_event.ch;
                size--;
                result++;
                buffer++;
            }
        }
    }
    return result;
}
#endif

__attribute__((section(".entry"))) void _start()
{
#if 0
    while(1) {
        kbd_read(&kbd_event);
        if(kbd_event.ch) {

        }
    }
#endif
}
