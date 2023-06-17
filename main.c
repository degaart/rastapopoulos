#include "util.h"
#include <stdarg.h>

#define PORT_COM1 0x3F8

static void serial_write_char(char ch)
{
    outb(PORT_COM1, ch);
}

static void serial_write_string(const char* s)
{
    while(*s) {
        serial_write_char(*s);
        s++;
    }
}

static void trace(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    while(*fmt) {
        switch(*fmt) {
            case '%':
                switch(*(fmt+1)) {
                    case 's':
                    {
                        const char* s = va_arg(args, const char*);
                        serial_write_string(s);
                        fmt++;
                        break;
                    }
                    case '%':
                    {
                        fmt++;
                        serial_write_char('%');
                        break;
                    }
                }
                break;
            case '\0':
                break;
            default:
                serial_write_char(*fmt);
                break;
        }
        fmt++;
    }
}

void kmain()
{
    /* Skip over bios boot messages */
    serial_write_string("\n");

    /* Write formatted string to debug output */
    const char* name = "Akhenathon";
    trace("Hello, %s\n", name);
}

