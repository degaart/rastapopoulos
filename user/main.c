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

#define KEY_LEFT  0xE04B
#define KEY_RIGHT 0xE04D
#define KEY_UP    0xE048
#define KEY_DOWN  0xE050
#define KEY_ESC   0x01

static volatile uint16_t* vga_buffer = (volatile uint16_t*)VGA_BASE;
int32_t syscall(uint32_t number, uint32_t param1, uint32_t param2,
                uint32_t param3, uint32_t param4, uint32_t param5);
int64_t syscall64(uint32_t number, uint32_t param1, uint32_t param2,
                  uint32_t param3, uint32_t param4, uint32_t param5);

static uint64_t sys_getticks(void)
{
    int64_t ticks = syscall64(0, 0, 0, 0, 0, 0);
    return ticks;
}

static void sys_sleep(uint32_t ticks)
{
    syscall(1, ticks, 0, 0, 0, 0);
}

static int sys_key_down(unsigned scancode)
{
    return syscall(2, scancode, 0, 0, 0, 0);
}

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
    if (x < 0 || x >= VGA_WIDTH)
        return;
    if (y < 0 || y >= VGA_HEIGHT)
        return;

    uint16_t attr = (fg << 4) | bg;
    vga_buffer[y * VGA_WIDTH + x] = (attr << 8) | ch;
}

void main(void)
{
    char buffer[16];
    uint32_t counter = 0;
    int fg = VGA_COLOR_BLACK;
    int bg = VGA_COLOR_WHITE;
    int x = VGA_WIDTH / 2;
    int y = 14;

    while (1) {
        uint32_t ticks = (uint32_t)sys_getticks();
        if (sys_key_down(KEY_LEFT))
            x--;
        if (sys_key_down(KEY_RIGHT))
            x++;
        if (sys_key_down(KEY_UP))
            y--;
        if (sys_key_down(KEY_DOWN))
            y++;

        if (x < 0)
            x = 0;
        if (x >= VGA_WIDTH)
            x = VGA_WIDTH - 1;
        if (y < 0)
            y = 0;
        if (y >= VGA_HEIGHT)
            y = VGA_HEIGHT - 1;

        size_t len = uint32_to_string(ticks, buffer, sizeof(buffer));
        int px = x - (len / 2);
        for (size_t i = 0; i < len; i++) {
            putchar(px + i, y, buffer[i], fg, bg);
        }
        sys_sleep(10);

        counter++;
        if (counter >= 100) {
            counter = 0;
            fg++;
            if (fg > VGA_COLOR_WHITE)
                fg = 0;
            bg--;
            if (bg < 0)
                bg = VGA_COLOR_WHITE;
        }
    }
}

