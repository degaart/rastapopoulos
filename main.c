#include "util.h"
#include <stdarg.h>
#include <stddef.h>

#define PORT_COM1 0x3F8
#define TRACE(fmt, ...) trace(__FILE__, __LINE__, __PRETTY_FUNCTION__, fmt "\n", __VA_ARGS__)

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

static void itoa(char* buffer, size_t size, unsigned value)
{
    char tmp[12];
    char* p = tmp;
    while(value) {
        *(p++) = (value % 10) + '0';
        value /= 10;
    }

    for(--p; p>=tmp && size > 1; size--) {
        *(buffer++) = *(p--);
    }
    *buffer = '\0';
}

static void trace(const char* file, int line, const char* fn, const char* fmt, ...)
{
    serial_write_char('[');
    serial_write_string(file);
    serial_write_char(':');

    serial_write_string(fn);
    serial_write_char(':');
    
    char line_buffer[16];
    itoa(line_buffer, sizeof(line_buffer), line);
    serial_write_string(line_buffer);
    serial_write_string("] ");

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
                    case 'u':
                    case 'd':
                    {
                        unsigned value = va_arg(args, unsigned);
                        char buffer[16];
                        itoa(buffer, sizeof(buffer), value);
                        serial_write_string(buffer);
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
    TRACE("Hello, %d", 8675309);
}

