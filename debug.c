#include "debug.h"
#include "string.h"
#include "util.h"
#include <stdarg.h>
#include <stddef.h>

#define PORT_COM1 0x3F8

void serial_write_char(char ch)
{
    outb(PORT_COM1, ch);
}

void serial_write_string(const char* s)
{
    while(*s) {
        serial_write_char(*s);
        s++;
    }
}

void trace_init()
{
    serial_write_char('\n');
}

void trace(const char* file, int line, const char* fn, const char* fmt, ...)
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
    serial_write_char('\n');
}

