#include <stddef.h>
#include <stdint.h>

#define VGA_BASE                0xb8000
#define VGA_COLOR_BLACK         0x0
#define VGA_COLOR_BLUE          0x1
#define VGA_COLOR_GREEN         0x2
#define VGA_COLOR_CYAN          0x3
#define VGA_COLOR_RED           0x4
#define VGA_COLOR_MAGENTA       0x5
#define VGA_COLOR_BROWN         0x6
#define VGA_COLOR_LIGHT_GREY    0x7
#define VGA_COLOR_DARK_GREY     0x8
#define VGA_COLOR_LIGHT_BLUE    0x9
#define VGA_COLOR_LIGHT_GREEN   0xA
#define VGA_COLOR_LIGHT_CYAN    0xB
#define VGA_COLOR_LIGHT_RED     0xC
#define VGA_COLOR_LIGHT_MAGENTA 0xD
#define VGA_COLOR_YELLOW        0xE
#define VGA_COLOR_WHITE         0xF

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

static volatile uint16_t* vga_buffer = (volatile uint16_t*)VGA_BASE;

static size_t uint32_to_string(uint32_t value, char* buffer, size_t buffer_len)
{
    static const char digits[] = "00010203040506070809"
                                 "10111213141516171819"
                                 "20212223242526272829"
                                 "30313233343536373839"
                                 "40414243444546474849"
                                 "50515253545556575859"
                                 "60616263646566676869"
                                 "70717273747576777879"
                                 "80818283848586878889"
                                 "90919293949596979899";

    if (buffer == NULL || buffer_len == 0) {
        return 0;
    }

    size_t length;

    if (value < 100000u) {
        if (value < 100u) {
            length = value < 10u ? 1 : 2;
        } else if (value < 10000u) {
            length = value < 1000u ? 3 : 4;
        } else {
            length = 5;
        }
    } else {
        if (value < 10000000u) {
            length = value < 1000000u ? 6 : 7;
        } else if (value < 1000000000u) {
            length = value < 100000000u ? 8 : 9;
        } else {
            length = 10;
        }
    }

    if (buffer_len <= length) {
        buffer[0] = '\0';
        return 0;
    }

    char* p = buffer + length;
    *p = '\0';

    while (value >= 100u) {
        uint32_t quotient = value / 100u;
        unsigned int index = (unsigned int)(value - quotient * 100u) * 2u;

        p -= 2;
        p[0] = digits[index];
        p[1] = digits[index + 1];

        value = quotient;
    }

    if (value < 10u) {
        *--p = (char)('0' + value);
    } else {
        unsigned int index = (unsigned int)value * 2u;

        p -= 2;
        p[0] = digits[index];
        p[1] = digits[index + 1];
    }

    return length;
}

static void putchar(int x, int y, int ch, int fg, int bg)
{
    uint16_t attr = (fg << 4) | bg;
    vga_buffer[y * VGA_WIDTH + x] = (attr << 8) | ch;
}

void main(void)
{
    char buffer[16];
    uint32_t counter = 0;
    while (1) {
        size_t len = uint32_to_string(counter, buffer, sizeof(buffer));
        int x = (VGA_WIDTH - len) / 2;
        int y = 14;
        for (size_t i = 0; i < len; i++) {
            putchar(x + i, y, buffer[i], VGA_COLOR_LIGHT_GREEN,
                    VGA_COLOR_BLACK);
        }
        counter++;
    }
}

