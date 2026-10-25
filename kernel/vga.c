#include "vga.h"
#include "kernel.h"
#include "kmalloc.h"
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

#define MODE_X_STRIDE     (VGA_MODE_X_WIDTH / 4)
#define MODE_X_PLANE_SIZE (MODE_X_STRIDE * VGA_MODE_X_HEIGHT)

struct Bitmap
{
    int width;
    int height;
    size_t stride;
    size_t plane_size;
    uint8_t* pixels;
};

static uint8_t backbuffer[4 * MODE_X_PLANE_SIZE] ALIGNED(4);
static struct Bitmap screen = {VGA_MODE_X_WIDTH, VGA_MODE_X_HEIGHT,
                               MODE_X_STRIDE, MODE_X_PLANE_SIZE, backbuffer};

static void fill_bytes(void* dst, size_t count, uint8_t color)
{
    size_t words = count >> 2;
    uint32_t value = color * 0x01010101u;
    asm volatile("rep stos{l|d}"
                 : "+D"(dst), "+c"(words)
                 : "a"(value)
                 : "memory");
    count &= 3;
    asm volatile("rep stosb" : "+D"(dst), "+c"(count) : "a"(color) : "memory");
}

static void copy_bytes(void* dst, const void* src, size_t count)
{
    size_t words = count >> 2;
    asm volatile("rep movs{l|d}"
                 : "+D"(dst), "+S"(src), "+c"(words)
                 :
                 : "memory");
    count &= 3;
    asm volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(count) : : "memory");
}

static uint8_t* bitmap_pixel(struct Bitmap* bitmap, int x, int y)
{
    return bitmap->pixels + (x & 3) * bitmap->plane_size + y * bitmap->stride +
           (x >> 2);
}

static void order_bounds(int* a, int* b)
{
    if (*a > *b) {
        int tmp = *a;
        *a = *b;
        *b = tmp;
    }
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

void vga_putpixel(unsigned x, unsigned y, uint8_t color)
{
    if (x >= VGA_MODE_X_WIDTH || y >= VGA_MODE_X_HEIGHT) {
        return;
    }

    backbuffer[(x & 3) * MODE_X_PLANE_SIZE + y * MODE_X_STRIDE + (x >> 2)] =
        color;
}

void vga_swap(void)
{
    for (unsigned plane = 0; plane < 4; ++plane) {
        vga_write_register(SEQ_IDX, SEQ_MAP_MASK, 1u << plane);
        copy_bytes((void*)VGA_START, backbuffer + plane * MODE_X_PLANE_SIZE,
                   MODE_X_PLANE_SIZE);
    }
}

void vga_clear(uint8_t color)
{
    fill_bytes(backbuffer, sizeof(backbuffer), color);
}

void vga_fillrect(int x1, int y1, int x2, int y2, uint8_t color)
{
    order_bounds(&x1, &x2);
    order_bounds(&y1, &y2);
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

    for (int plane = 0; plane < 4; ++plane) {
        int first = x1 + ((plane - x1) & 3);
        if (first > x2)
            continue;
        size_t count = ((x2 - first) >> 2) + 1;
        uint8_t* dst = bitmap_pixel(&screen, first, y1);
        if (count == MODE_X_STRIDE) {
            fill_bytes(dst, count * (y2 - y1 + 1), color);
        } else {
            for (int y = y1; y <= y2; ++y, dst += MODE_X_STRIDE)
                fill_bytes(dst, count, color);
        }
    }
}

void vga_rect(int x1, int y1, int x2, int y2, uint8_t color)
{
    order_bounds(&x1, &x2);
    order_bounds(&y1, &y2);
    vga_fillrect(x1, y1, x2, y1, color);
    vga_fillrect(x1, y2, x2, y2, color);
    vga_fillrect(x1, y1, x1, y2, color);
    vga_fillrect(x2, y1, x2, y2, color);
}

void vga_drawline(int x1, int y1, int x2, int y2, uint8_t color)
{
    if (x1 == x2 || y1 == y2) {
        vga_fillrect(x1, y1, x2, y2, color);
        return;
    }

    int64_t dx = (int64_t)x2 - x1;
    int64_t dy = (int64_t)y2 - y1;
    bool steep = (dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx);
    int major1 = steep ? y1 : x1;
    int major2 = steep ? y2 : x2;
    int minor1 = steep ? x1 : y1;
    int minor2 = steep ? x2 : y2;
    if (major1 > major2) {
        order_bounds(&major1, &major2);
        int tmp = minor1;
        minor1 = minor2;
        minor2 = tmp;
    }
    int limit = steep ? VGA_MODE_X_HEIGHT : VGA_MODE_X_WIDTH;
    int first = major1 < 0 ? 0 : major1;
    int last = major2 >= limit ? limit - 1 : major2;
    if (first > last)
        return;

    uint32_t major = (int64_t)major2 - major1;
    int64_t delta = (int64_t)minor2 - minor1;
    int step = delta < 0 ? -1 : 1;
    uint32_t minor = delta < 0 ? -delta : delta;
    uint64_t phase = (uint64_t)((int64_t)first - major1) * minor + major / 2;
    int64_t pos = minor1 + (int64_t)(phase / major) * step;
    uint64_t error = phase % major;
    for (int i = first; i <= last; ++i) {
        if (steep)
            vga_putpixel(pos, i, color);
        else
            vga_putpixel(i, pos, color);
        error += minor;
        if (error >= major) {
            error -= major;
            pos += step;
        }
    }
}

void vga_oval(int x1, int y1, int x2, int y2, uint8_t color)
{
    order_bounds(&x1, &x2);
    order_bounds(&y1, &y2);
    if (x2 < 0 || y2 < 0 || x1 >= VGA_MODE_X_WIDTH || y1 >= VGA_MODE_X_HEIGHT)
        return;
    if (x1 == x2 || y1 == y2) {
        vga_fillrect(x1, y1, x2, y2, color);
        return;
    }

    int64_t a = (int64_t)x2 - x1;
    int64_t b = (int64_t)y2 - y1;
    int64_t odd = b & 1;
    int64_t dx = 4 * (1 - a) * b * b;
    int64_t dy = 4 * (odd + 1) * a * a;
    int64_t error = dx + dy + odd * a * a;
    y1 += (b + 1) / 2;
    y2 = y1 - odd;
    a *= 8 * a;
    odd = 8 * b * b;

    do {
        vga_putpixel(x2, y1, color);
        vga_putpixel(x1, y1, color);
        vga_putpixel(x1, y2, color);
        vga_putpixel(x2, y2, color);
        int64_t twice = 2 * error;
        if (twice <= dy) {
            ++y1;
            --y2;
            dy += a;
            error += dy;
        }
        if (twice >= dx || 2 * error > dy) {
            ++x1;
            --x2;
            dx += odd;
            error += dx;
        }
    } while (x1 <= x2);

    while ((int64_t)y1 - y2 <= b) {
        vga_putpixel(x1 - 1, y1, color);
        vga_putpixel(x2 + 1, y1++, color);
        vga_putpixel(x1 - 1, y2, color);
        vga_putpixel(x2 + 1, y2--, color);
    }
}

void vga_roundrect(int x1, int y1, int x2, int y2, int radius, uint8_t color)
{
    order_bounds(&x1, &x2);
    order_bounds(&y1, &y2);
    if (x2 < 0 || y2 < 0 || x1 >= VGA_MODE_X_WIDTH || y1 >= VGA_MODE_X_HEIGHT)
        return;
    int64_t max_radius = ((int64_t)x2 - x1) / 2;
    if (((int64_t)y2 - y1) / 2 < max_radius)
        max_radius = ((int64_t)y2 - y1) / 2;
    if (radius > max_radius)
        radius = max_radius;
    if (radius <= 0) {
        vga_rect(x1, y1, x2, y2, color);
        return;
    }

    int left = x1 + radius;
    int right = x2 - radius;
    int top = y1 + radius;
    int bottom = y2 - radius;
    vga_fillrect(left, y1, right, y1, color);
    vga_fillrect(left, y2, right, y2, color);
    vga_fillrect(x1, top, x1, bottom, color);
    vga_fillrect(x2, top, x2, bottom, color);
    int x = radius;
    int y = 0;
    int64_t error = 1 - radius;
    while (x >= y) {
        vga_putpixel(left - x, top - y, color);
        vga_putpixel(left - y, top - x, color);
        vga_putpixel(right + x, top - y, color);
        vga_putpixel(right + y, top - x, color);
        vga_putpixel(left - x, bottom + y, color);
        vga_putpixel(left - y, bottom + x, color);
        vga_putpixel(right + x, bottom + y, color);
        vga_putpixel(right + y, bottom + x, color);
        ++y;
        if (error < 0) {
            error += 2 * (int64_t)y + 1;
        } else {
            --x;
            error += 2 * ((int64_t)y - x) + 1;
        }
    }
}

static bool clip_blit_axis(int* src, int src_limit, int* dst, int dst_limit,
                           int* count)
{
    int64_t first = 0;
    int64_t last = *count;
    if (*src < 0)
        first = -(int64_t)*src;
    if (-(int64_t)*dst > first)
        first = -(int64_t)*dst;
    if ((int64_t)src_limit - *src < last)
        last = (int64_t)src_limit - *src;
    if ((int64_t)dst_limit - *dst < last)
        last = (int64_t)dst_limit - *dst;
    if (first >= last)
        return false;
    *src += first;
    *dst += first;
    *count = last - first;
    return true;
}

void vga_blit(struct Bitmap* src, int x1, int y1, struct Bitmap* dst, int x2,
              int y2, int width, int height)
{
    if (!src)
        src = &screen;
    if (!dst)
        dst = &screen;
    if (!clip_blit_axis(&x1, src->width, &x2, dst->width, &width) ||
        !clip_blit_axis(&y1, src->height, &y2, dst->height, &height))
        return;

    bool overlap = src == dst && x2 < x1 + width && x1 < x2 + width &&
                   y2 < y1 + height && y1 < y2 + height;
    int row = overlap && y2 > y1 ? height - 1 : 0;
    int step = overlap && y2 > y1 ? -1 : 1;

    /* A shifted self-blit can overwrite a source plane not yet copied. */
    if (overlap && ((x1 ^ x2) & 3)) {
        int first = x2 > x1 ? width - 1 : 0;
        int dx = x2 > x1 ? -1 : 1;
        for (int n = 0; n < height; ++n, row += step) {
            int x = first;
            for (int i = 0; i < width; ++i, x += dx)
                *bitmap_pixel(dst, x2 + x, y2 + row) =
                    *bitmap_pixel(src, x1 + x, y1 + row);
        }
        return;
    }

    for (int plane = 0; plane < 4 && plane < width; ++plane) {
        size_t count = ((width - 1 - plane) >> 2) + 1;
        const uint8_t* in = bitmap_pixel(src, x1 + plane, y1 + row);
        uint8_t* out = bitmap_pixel(dst, x2 + plane, y2 + row);
        if (!overlap && count == src->stride && count == dst->stride) {
            copy_bytes(out, in, count * height);
            continue;
        }
        for (int n = 0; n < height; ++n) {
            if (overlap && out > in && (size_t)(out - in) < count) {
                for (size_t i = count; i; --i)
                    out[i - 1] = in[i - 1];
            } else {
                copy_bytes(out, in, count);
            }
            if (n + 1 < height) {
                in += step * (ptrdiff_t)src->stride;
                out += step * (ptrdiff_t)dst->stride;
            }
        }
    }
}

struct Bitmap* vga_load_bitmap(const void* buffer, size_t len, int width,
                               int height)
{
    if (width <= 0 || height <= 0 || (size_t)width > len / (size_t)height)
        return NULL;
    size_t stride = ((size_t)width + 3) >> 2;
    if (stride > (SIZE_MAX - sizeof(struct Bitmap)) / 4 / (size_t)height)
        return NULL;
    size_t plane_size = stride * height;
    struct Bitmap* bitmap = kmalloc(sizeof(*bitmap) + 4 * plane_size);
    if (!bitmap)
        return NULL;
    *bitmap = (struct Bitmap) {width, height, stride, plane_size,
                               (uint8_t*)(bitmap + 1)};
    const uint8_t* pixels = buffer;
    for (int plane = 0; plane < 4; ++plane) {
        uint8_t* out = bitmap->pixels + plane * plane_size;
        for (int y = 0; y < height; ++y) {
            const uint8_t* in = pixels + (size_t)y * width;
            for (int x = plane; x < width; x += 4)
                out[x >> 2] = in[x];
            out += stride;
        }
    }
    return bitmap;
}

void vga_destroy_bitmap(struct Bitmap* bitmap)
{
    kfree(bitmap);
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
    vga_clear(0);
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

