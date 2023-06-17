#include "util.h"
#include "string.h"
#include <stdarg.h>
#include <stddef.h>

#define PORT_COM1 0x3F8
#define TRACE(fmt, ...) trace(__FILE__, __LINE__, __PRETTY_FUNCTION__, fmt "\n", #__VA_ARGS__)

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

static void itox(char* buffer, size_t size, unsigned value)
{
    char tmp[12];
    char* p = tmp;
    while(value) {
        int digit = value % 16;
        *(p++) = digit + (digit < 10 ? '0' : 'A' - 10);
        value /= 16;
    }

    for(--p; p>=tmp && size > 1; size--) {
        *(buffer++) = *(p--);
    }
    *buffer = '\0';
}

static void itoa(char* buffer, size_t size, unsigned value)
{
    char tmp[9];
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
                    case 'x':
                    case 'X':
                    {
                        unsigned value = va_arg(args, unsigned);
                        char buffer[16];
                        itox(buffer, sizeof(buffer), value);
                        serial_write_string(buffer);
                        fmt++;
                        break;
                    }
                    case 'p':
                    case 'P':
                    {
                        unsigned value = va_arg(args, unsigned);
                        char buffer[16];
                        itox(buffer, sizeof(buffer), value);
                        int pad = 8 - strlen(buffer);
                        serial_write_string("0x");
                        for(int i = 0; i < pad; i++)
                            serial_write_char('0');
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

void kmain(const void* multiboot_info, uint32_t multiboot_magic)
{
    /* Skip over bios boot messages */
    serial_write_string("\n");

    /* Write formatted string to debug output */
    if(multiboot_magic != 0x2BADB002) {
        TRACE("PANIC: Bad multiboot magic");
        while(1);
    }
}

