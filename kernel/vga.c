#include "vga.h"
#include "kernel.h"
#include "process.h"
#include "trace.h"
#include "vmm.h"
#include <stddef.h>

#define ATTR_IDX_DATA 0x3C0
#define MISC_OUTPUT   0x3C2
#define SEQ_IDX       0x3C4
#define DAC_MASK      0x3C6
#define DAC_WRITE_IDX 0x3C8
#define DAC_DATA      0x3C9
#define GC_IDX        0x3CE
#define CRTC_IDX      0x3D4
#define CRTC_DATA     0x3D5
#define INPUT_STAT_1  0x3DA

#define SEQ_CLK_MODE  0x01
#define SEQ_MAP_MASK  0x02
#define SEQ_MEM_MODE  0x04
#define SEQ_REG_COUNT 5

#define CRTC_CURSOR_START 0x0A
#define CRTC_CURSOR_END   0x0B
#define CRTC_CURSOR_HIGH  0x0E
#define CRTC_CURSOR_LOW   0x0F
#define CRTC_VRETRACE_END 0x11
#define CRTC_REG_COUNT    25

#define GC_READ_MAP_SEL 0x04
#define GC_MODE         0x05
#define GC_MISC         0x06
#define GC_REG_COUNT    9

#define ATTR_MODE_CTRL      0x10
#define ATTR_OVERSCAN_COLOR 0x11
#define ATTR_PLANE_EN       0x12
#define ATTR_HORIZ_PAN      0x13
#define ATTR_COLOR_SEL      0x14
#define ATTR_PALETTE_SIZE   16
#define ATTR_REG_COUNT      21
#define ATTR_DISPLAY_EN     0x20
#define BACKSPACE           0x08
#define FONT_SIZE           4096
#define MEMORY              ((volatile uint8_t*)VGA_START)

static volatile uint16_t* vga_buffer = (volatile uint16_t*)VGA_BASE;
static struct VGAPoint cursor;
static struct Process* locked_process;
static uint8_t text_font[FONT_SIZE];

struct VGAPoint vga_get_cursor_pos(void)
{
    uint16_t pos;

    outb(CRTC_IDX, CRTC_CURSOR_HIGH);
    pos = (uint16_t)inb(CRTC_DATA) << 8;

    outb(CRTC_IDX, CRTC_CURSOR_LOW);
    pos |= inb(CRTC_DATA);

    struct VGAPoint result = {.x = pos % VGA_WIDTH, .y = pos / VGA_WIDTH};
    return result;
}

void vga_set_cursor_pos(struct VGAPoint p)
{
    if (p.x >= VGA_WIDTH)
        p.x = VGA_WIDTH - 1;
    if (p.y >= VGA_HEIGHT)
        p.y = VGA_HEIGHT - 1;
    uint16_t pos = (uint16_t)p.y * VGA_WIDTH + p.x;

    outb(CRTC_IDX, CRTC_CURSOR_HIGH);
    outb(CRTC_DATA, (uint8_t)(pos >> 8));

    outb(CRTC_IDX, CRTC_CURSOR_LOW);
    outb(CRTC_DATA, (uint8_t)(pos & 0xFF));
}

void vga_write(const char* text, int len, uint8_t fg, uint8_t bg)
{
    bool iflag = enter_critical_section();
    if (locked_process) {
        panic("%s called while screen was locked", __FUNCTION__);
        leave_critical_section(iflag);
        return;
    }

    const uint16_t attr = (bg << 4) | fg;
    while ((len == -1 && *text) || len > 0) {
        char c = *text++;
        if (c == BACKSPACE) {
            if (cursor.x) {
                cursor.x--;
            } else {
                cursor.x = VGA_WIDTH - 1;
                if (cursor.y)
                    cursor.y--;
            }
            vga_write_at(cursor.x, cursor.y, ' ', fg, bg);
        } else if (c == '\n') {
            cursor.x = 0;
            cursor.y++;
        } else {
            vga_buffer[cursor.y * VGA_WIDTH + cursor.x] =
                (attr << 8) | (uint8_t)c;

            cursor.x++;

            if (cursor.x >= VGA_WIDTH) {
                cursor.x = 0;
                cursor.y++;
            }
        }

        if (cursor.y >= VGA_HEIGHT) {
            /* Scroll screen up by one row. */
            for (size_t i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); ++i) {
                vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
            }

            /* Clear the last row. */
            for (size_t i = VGA_WIDTH * (VGA_HEIGHT - 1);
                 i < VGA_WIDTH * VGA_HEIGHT; ++i) {
                vga_buffer[i] = (attr << 8) | ' ';
            }

            cursor.y = VGA_HEIGHT - 1;
        }

        if (len != -1)
            len--;
    }
    vga_set_cursor_pos(cursor);
}

void vga_write_at(int x, int y, char ch, uint8_t fg, uint8_t bg)
{
    uint16_t attr = (bg << 4) | fg;
    vga_buffer[y * VGA_WIDTH + x] = (attr << 8) | (uint8_t)ch;
}

static void vga_write_register(uint16_t port, uint8_t index, uint8_t value)
{
    outb(port, index);
    outb(port + 1, value);
}

static void vga_attribute(uint8_t index, uint8_t value)
{
    inb(INPUT_STAT_1);
    outb(ATTR_IDX_DATA, index);
    outb(ATTR_IDX_DATA, value);
}

void vga_putpixel(unsigned x, unsigned y, uint8_t color)
{
    if (x >= VGA_MODE_X_WIDTH || y >= VGA_MODE_X_HEIGHT) {
        return;
    }

    vga_write_register(SEQ_IDX, SEQ_MAP_MASK, 1u << (x & 3));

    MEMORY[y * (VGA_MODE_X_WIDTH / 4) + (x >> 2)] = color;
}

void vga_set_palette(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    outb(DAC_WRITE_IDX, index);
    outb(DAC_DATA, red & 63);
    outb(DAC_DATA, green & 63);
    outb(DAC_DATA, blue & 63);
}

void vga_mode_x(void)
{
    static const uint8_t sequencer[SEQ_REG_COUNT] = {0x03, 0x01, 0x0F, 0x00,
                                                     0x06};

    static const uint8_t crtc[CRTC_REG_COUNT] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0x0D, 0x3E, 0x00,
        0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEA, 0xAC,
        0xDF, 0x28, 0x00, 0xE7, 0x06, 0xE3, 0xFF};

    static const uint8_t graphics[GC_REG_COUNT] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF};

    outb(MISC_OUTPUT, 0xE3);

    vga_write_register(SEQ_IDX, SEQ_CLK_MODE, 0x21);

    for (unsigned i = 0; i < SEQ_REG_COUNT; ++i) {
        if (i != SEQ_CLK_MODE) {
            vga_write_register(SEQ_IDX, i, sequencer[i]);
        }
    }

    vga_write_register(CRTC_IDX, CRTC_VRETRACE_END, 0x2C);

    for (unsigned i = 0; i < CRTC_REG_COUNT; ++i) {
        if (i == CRTC_VRETRACE_END) {
            continue;
        }

        vga_write_register(CRTC_IDX, i, crtc[i]);
    }

    vga_write_register(CRTC_IDX, CRTC_VRETRACE_END, crtc[CRTC_VRETRACE_END]);

    for (unsigned i = 0; i < GC_REG_COUNT; ++i) {
        vga_write_register(GC_IDX, i, graphics[i]);
    }

    for (unsigned i = 0; i < ATTR_PALETTE_SIZE; ++i) {
        vga_attribute(i, i);
    }

    vga_attribute(ATTR_MODE_CTRL, 0x41);
    vga_attribute(ATTR_OVERSCAN_COLOR, 0x00);
    vga_attribute(ATTR_PLANE_EN, 0x0F);
    vga_attribute(ATTR_HORIZ_PAN, 0x00);
    vga_attribute(ATTR_COLOR_SEL, 0x00);

    outb(DAC_MASK, 0xFF);
    for (unsigned i = 0; i < 256; ++i) {
        vga_set_palette(i, ((i >> 5) & 7) * 63 / 7, ((i >> 2) & 7) * 63 / 7,
                        (i & 3) * 63 / 3);
    }

    vga_write_register(SEQ_IDX, SEQ_MAP_MASK, 0x0F);

    for (size_t i = 0; i < 65536; ++i) {
        MEMORY[i] = 0;
    }

    vga_write_register(SEQ_IDX, SEQ_MAP_MASK, 0x0F);
    vga_write_register(SEQ_IDX, SEQ_CLK_MODE, 0x01);

    inb(INPUT_STAT_1);
    outb(ATTR_IDX_DATA, ATTR_DISPLAY_EN);
}

static void vga_save_text_font(uint8_t* font)
{
    volatile uint8_t* memory = (volatile uint8_t*)VGA_START;

    vga_write_register(SEQ_IDX, SEQ_MAP_MASK, 0x04);
    vga_write_register(SEQ_IDX, SEQ_MEM_MODE, 0x06);

    vga_write_register(GC_IDX, GC_READ_MAP_SEL, 0x02);
    vga_write_register(GC_IDX, GC_MODE, 0x00);
    vga_write_register(GC_IDX, GC_MISC, 0x04);

    for (unsigned c = 0; c < 256; ++c) {
        for (unsigned y = 0; y < 16; ++y) {
            font[c * 16 + y] = memory[c * 32 + y];
        }
    }

    vga_write_register(SEQ_IDX, SEQ_MAP_MASK, 0x03);
    vga_write_register(SEQ_IDX, SEQ_MEM_MODE, 0x02);

    vga_write_register(GC_IDX, GC_READ_MAP_SEL, 0x00);
    vga_write_register(GC_IDX, GC_MODE, 0x10);
    vga_write_register(GC_IDX, GC_MISC, 0x0E);
}

static void vga_restore_text_palette(void)
{
    outb(DAC_MASK, 0xFF);
    outb(DAC_WRITE_IDX, 0);

    for (unsigned i = 0; i < 64; ++i) {
        uint8_t red = ((i & 0x04) ? 42 : 0) + ((i & 0x20) ? 21 : 0);

        uint8_t green = ((i & 0x02) ? 42 : 0) + ((i & 0x10) ? 21 : 0);

        uint8_t blue = ((i & 0x01) ? 42 : 0) + ((i & 0x08) ? 21 : 0);

        outb(DAC_DATA, red);
        outb(DAC_DATA, green);
        outb(DAC_DATA, blue);
    }
}

void vga_text_mode(void)
{
    static const uint8_t sequencer[SEQ_REG_COUNT] = {0x03, 0x00, 0x03, 0x00,
                                                     0x02};

    static const uint8_t crtc[CRTC_REG_COUNT] = {
        0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F, 0x00,
        0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50, 0x9C, 0x0E,
        0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF};

    static const uint8_t graphics[GC_REG_COUNT] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF};

    static const uint8_t attribute[ATTR_REG_COUNT] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39, 0x3A,
        0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x0C, 0x00, 0x0F, 0x08, 0x00};

    vga_write_register(SEQ_IDX, SEQ_CLK_MODE, 0x20);

    outb(MISC_OUTPUT, 0x67);

    vga_write_register(CRTC_IDX, CRTC_VRETRACE_END, 0x0E);

    for (unsigned i = 0; i < SEQ_REG_COUNT; ++i) {
        if (i != SEQ_CLK_MODE) {
            vga_write_register(SEQ_IDX, i, sequencer[i]);
        }
    }

    for (unsigned i = 0; i < CRTC_REG_COUNT; ++i) {
        vga_write_register(CRTC_IDX, i, crtc[i]);
    }

    for (unsigned i = 0; i < GC_REG_COUNT; ++i) {
        vga_write_register(GC_IDX, i, graphics[i]);
    }

    for (unsigned i = 0; i < ATTR_REG_COUNT; ++i) {
        vga_attribute(i, attribute[i]);
    }

    vga_restore_text_palette();

    volatile uint8_t* memory = (volatile uint8_t*)VGA_START;

    vga_write_register(SEQ_IDX, SEQ_MAP_MASK, 0x04);
    vga_write_register(SEQ_IDX, SEQ_MEM_MODE, 0x06);

    vga_write_register(GC_IDX, GC_READ_MAP_SEL, 0x02);
    vga_write_register(GC_IDX, GC_MODE, 0x00);
    vga_write_register(GC_IDX, GC_MISC, 0x04);

    for (unsigned c = 0; c < 256; ++c) {
        for (unsigned y = 0; y < 32; ++y) {
            memory[c * 32 + y] = y < 16 ? text_font[c * 16 + y] : 0;
        }
    }

    vga_write_register(SEQ_IDX, SEQ_MAP_MASK, 0x03);
    vga_write_register(SEQ_IDX, SEQ_MEM_MODE, 0x02);

    vga_write_register(GC_IDX, GC_READ_MAP_SEL, 0x00);
    vga_write_register(GC_IDX, GC_MODE, 0x10);
    vga_write_register(GC_IDX, GC_MISC, 0x0E);

    volatile uint16_t* screen = (volatile uint16_t*)VGA_BASE;

    for (unsigned i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        screen[i] = 0x0720;
    }

    vga_write_register(CRTC_IDX, CRTC_CURSOR_START, 0x0D);
    vga_write_register(CRTC_IDX, CRTC_CURSOR_END, 0x0E);
    vga_write_register(CRTC_IDX, CRTC_CURSOR_HIGH, 0x00);
    vga_write_register(CRTC_IDX, CRTC_CURSOR_LOW, 0x00);
    cursor.x = cursor.y = 0;

    vga_write_register(SEQ_IDX, SEQ_CLK_MODE, sequencer[SEQ_CLK_MODE]);

    inb(INPUT_STAT_1);
    outb(ATTR_IDX_DATA, ATTR_DISPLAY_EN);
}

void vga_init(void)
{
    cursor = vga_get_cursor_pos();
    vga_save_text_font(text_font);
}

bool vga_lock_screen(void)
{
    bool iflag = enter_critical_section();
    if (locked_process) {
        leave_critical_section(iflag);
        return false;
    }

    locked_process = process_current();
    process_incref(locked_process);

    for (void* ptr = (void*)VGA_PHYSICAL_START; ptr < (void*)VGA_PHYSICAL_END;
         ptr += PAGE_SIZE) {
        if (ptr != (void*)VGA_PHYSICAL_BASE) {
            if (!vmm_map(ptr, ptr,
                         VMM_USER | VMM_WRITABLE | VMM_UNCACHED |
                             VMM_SHARED)) {
                trace("Failed to map page 0x%p", ptr);
                process_decref(locked_process);
                locked_process = NULL;
                leave_critical_section(iflag);
                return false;
            }
        }
    }

    vga_mode_x();

    leave_critical_section(iflag);
    return true;
}

bool vga_unlock_screen(void)
{
    bool iflag = enter_critical_section();
    if (locked_process != process_current()) {
        leave_critical_section(iflag);
        return false;
    }

    process_decref(locked_process);
    locked_process = NULL;

    vga_text_mode();

    for (void* ptr = (void*)VGA_PHYSICAL_START; ptr < (void*)VGA_PHYSICAL_END;
         ptr += PAGE_SIZE) {
        if (ptr != (void*)VGA_PHYSICAL_BASE) {
            if (!vmm_unmap(ptr)) {
                trace("Failed to unmap page 0x%p", ptr);
            }
        }
    }

    leave_critical_section(iflag);
    return true;
}

