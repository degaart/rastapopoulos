#include "vga.h"
#include "address_space.h"
#include "kernel.h"
#include "kmalloc.h"
#include "pit.h"
#include "process.h"
#include "trace.h"
#include "vmm.h"
#include <stddef.h>
#include <string.h>

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

#define MODE_X_STRIDE     (VGA_MODE_X_WIDTH / 4)
#define MODE_X_PLANE_SIZE (MODE_X_STRIDE * VGA_MODE_X_HEIGHT)

#define SHADOW_SIZE  (VGA_MODE_X_WIDTH * VGA_MODE_X_HEIGHT)
#define SHADOW_PAGES ((SHADOW_SIZE + PAGE_SIZE - 1) / PAGE_SIZE)
static void* shadow_allocation;
static uint8_t* shadow;
static unsigned visible_page;
static unsigned profile_swaps, profile_bytes;
static uint32_t profile_convert, profile_upload, profile_max;
static uint8_t plane_buffer[MODE_X_PLANE_SIZE] ALIGNED(4);
struct VGADamage
{
    int x1, y1, x2, y2;
};
static struct VGADamage damage[2];
extern void vga_pack_plane(uint8_t* dst, const uint8_t* src, unsigned count);
extern void vga_pack_rows(uint8_t* dst, const uint8_t* src, unsigned count,
                          unsigned rows);
extern void vga_copy_rows(uint8_t* dst, const uint8_t* src, unsigned count,
                          unsigned rows);

static void damage_add(struct VGADamage* d, int x1, int y1, int x2, int y2)
{
    if (x1 < d->x1)
        d->x1 = x1;
    if (y1 < d->y1)
        d->y1 = y1;
    if (x2 > d->x2)
        d->x2 = x2;
    if (y2 > d->y2)
        d->y2 = y2;
}

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

void vga_swap_rect(int x1, int y1, int x2, int y2)
{
    if (!shadow || locked_process != process_current())
        return;
    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;
    if (x2 >= VGA_MODE_X_WIDTH)
        x2 = VGA_MODE_X_WIDTH - 1;
    if (y2 >= VGA_MODE_X_HEIGHT)
        y2 = VGA_MODE_X_HEIGHT - 1;
    if (x1 > x2 || y1 > y2)
        return;
    uint32_t begin = pit_perf_clock();
    damage_add(&damage[0], x1, y1, x2, y2);
    damage_add(&damage[1], x1, y1, x2, y2);
    unsigned page = visible_page ^ 1;
    unsigned offset = page * MODE_X_PLANE_SIZE;
    struct VGADamage* d = &damage[page];
    unsigned rows = d->y2 - d->y1 + 1;
    for (unsigned plane = 0; plane < 4; ++plane) {
        int first = d->x1 + ((plane - d->x1) & 3);
        if (first > d->x2)
            continue;
        unsigned count = ((d->x2 - first) >> 2) + 1;
        const uint8_t* src = shadow + d->y1 * VGA_MODE_X_WIDTH + first;
        uint32_t start = pit_perf_clock();
        if (count == MODE_X_STRIDE) {
            unsigned total = count * rows;
            for (unsigned i = 0; i < total; i += 1024) {
                unsigned n = total - i;
                if (n > 1024)
                    n = 1024;
                vga_pack_plane(plane_buffer + i, src + i * 4, n);
                pit_perf_clock();
            }
        } else {
            vga_pack_rows(plane_buffer, src, count, rows);
        }
        uint32_t converted = pit_perf_clock();
        vga_write_register(SEQ_IDX, SEQ_MAP_MASK, 1u << plane);
        uint8_t* dst = (uint8_t*)VGA_START + offset + d->y1 * MODE_X_STRIDE +
                       (first >> 2);
        if (count == MODE_X_STRIDE) {
            unsigned total = count * rows;
            for (unsigned i = 0; i < total; i += 4096) {
                unsigned n = total - i;
                if (n > 4096)
                    n = 4096;
                memcpy(dst + i, plane_buffer + i, n);
                pit_perf_clock();
            }
        } else {
            vga_copy_rows(dst, plane_buffer, count, rows);
        }
        profile_convert += converted - start;
        profile_upload += (uint32_t)pit_perf_clock() - converted;
        profile_bytes += count * rows;
    }
    *d = (struct VGADamage) {VGA_MODE_X_WIDTH, VGA_MODE_X_HEIGHT, -1, -1};
    vga_write_register(CRTC_IDX, 0x0c, offset >> 8);
    vga_write_register(CRTC_IDX, 0x0d, offset & 255);
    visible_page = page;
    ++profile_swaps;
    uint32_t duration = (uint32_t)pit_perf_clock() - begin;
    if (duration > profile_max)
        profile_max = duration;
}

void vga_swap(void)
{
    vga_swap_rect(0, 0, VGA_MODE_X_WIDTH - 1, VGA_MODE_X_HEIGHT - 1);
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
    visible_page = 0;
    damage[0] = damage[1] =
        (struct VGADamage) {VGA_MODE_X_WIDTH, VGA_MODE_X_HEIGHT, -1, -1};
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
    shadow_allocation = kmalloc(SHADOW_PAGES * PAGE_SIZE + PAGE_SIZE - 1);
    if (!shadow_allocation) {
        leave_critical_section(iflag);
        return false;
    }
    shadow = (uint8_t*)(((uintptr_t)shadow_allocation + PAGE_SIZE - 1) &
                        ~(uintptr_t)(PAGE_SIZE - 1));
    memset(shadow, 0, SHADOW_PAGES * PAGE_SIZE);
    unsigned mapped = 0;
    for (; mapped < SHADOW_PAGES; ++mapped) {
        void* frame;
        if (!vmm_frame(shadow + mapped * PAGE_SIZE, &frame) ||
            !vmm_map(frame, (void*)(VGA_PHYSICAL_START + mapped * PAGE_SIZE),
                     VMM_USER | VMM_WRITABLE | VMM_SHARED)) {
            while (mapped)
                vmm_unmap((void*)(VGA_PHYSICAL_START + --mapped * PAGE_SIZE));
            kfree(shadow_allocation);
            shadow_allocation = NULL;
            shadow = NULL;
            leave_critical_section(iflag);
            return false;
        }
    }
    locked_process = process_current();
    process_incref(locked_process);
    vga_mode_x();
    profile_swaps = profile_bytes = 0;
    profile_convert = profile_upload = profile_max = 0;
    leave_critical_section(iflag);
    return true;
}

void vga_cleanup_process(struct Process* owner)
{
    bool iflag = enter_critical_section();
    if (locked_process != owner) {
        leave_critical_section(iflag);
        return;
    }
    for (unsigned i = 0; i < SHADOW_PAGES; ++i) {
        if (!address_space_unmap(owner->as,
                                 (void*)(VGA_PHYSICAL_START + i * PAGE_SIZE)))
            panic("Failed to unmap VGA shadow page");
    }
    kfree(shadow_allocation);
    shadow_allocation = NULL;
    shadow = NULL;
    locked_process = NULL;
    vga_text_mode();
    process_decref(owner);
    leave_critical_section(iflag);
}

bool vga_unlock_screen(void)
{
    bool iflag = enter_critical_section();
    if (locked_process != process_current()) {
        leave_critical_section(iflag);
        return false;
    }
    trace("VGA perf: swaps=%u bytes=%u convert_clocks=%u upload_clocks=%u "
          "max_clocks=%u",
          profile_swaps, profile_bytes, (unsigned)profile_convert,
          (unsigned)profile_upload, (unsigned)profile_max);
    vga_cleanup_process(locked_process);
    leave_critical_section(iflag);
    return true;
}
