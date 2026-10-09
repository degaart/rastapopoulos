#include "kernel.h"
#include <stdint.h>

#define COM1 0x3F8

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01); /* 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* 8 data bits, no parity, 1 stop bit */
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x03);
}

static int serial_can_write(void)
{
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c)
{
    while (!serial_can_write()) {
    }

    outb(COM1, (uint8_t)c);
}

void serial_write(const char* text)
{
    while (*text) {
        serial_putchar(*text++);
    }
}

