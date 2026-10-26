#ifdef RASTA_KERNEL
#    include "../address_space.h"
#    include "../kernel.h"
#    include "../process.h"
#    include "../trace.h"
#    include "../vga.h"
#    include "../vmm.h"

#    define CHECK(cond)                                                       \
        do {                                                                  \
            if (!(cond)) {                                                    \
                trace("VGA check failed: %s", #cond);                         \
                debugexit(2);                                                 \
            }                                                                 \
        } while (0)

static void check_visible(const uint8_t* pixels)
{
    outb(0x3d4, 0x0c);
    unsigned offset = (unsigned)inb(0x3d5) << 8;
    outb(0x3d4, 0x0d);
    offset |= inb(0x3d5);
    for (unsigned plane = 0; plane < 4; ++plane) {
        outb(0x3ce, 4);
        outb(0x3cf, plane);
        const volatile uint8_t* src =
            (const volatile uint8_t*)VGA_START + offset;
        for (unsigned i = 0; i < 19200; ++i)
            CHECK(src[i] == pixels[i * 4 + plane]);
    }
}

extern void vga_pack_plane(uint8_t* dst, const uint8_t* src, unsigned count);
static void check_packing(void)
{
    uint8_t source[4 * 100 + 4];
    uint8_t target[104];
    for (unsigned i = 0; i < sizeof(source); ++i)
        source[i] = i * 13 + i / 3;
    for (unsigned count = 0; count <= 100; ++count) {
        for (unsigned alignment = 0; alignment < 4; ++alignment) {
            for (unsigned i = 0; i < sizeof(target); ++i)
                target[i] = 0x53;
            vga_pack_plane(target + alignment, source + alignment, count);
            for (unsigned i = 0; i < sizeof(target); ++i)
                CHECK(target[i] ==
                      (i >= alignment && i < alignment + count
                           ? source[alignment + (i - alignment) * 4]
                           : 0x53));
        }
    }
}

void test_vga(void)
{
    check_packing();
    CHECK(vga_lock_screen());
    CHECK(!vga_lock_screen());
    CHECK(vmm_is_valid_userptr((void*)VGA_PHYSICAL_START, 320 * 240, true));
    for (unsigned i = 0; i < 19; ++i) {
        unsigned flags;
        void* frame;
        CHECK(address_space_query(process_current()->as,
                                  (void*)(VGA_PHYSICAL_START + i * PAGE_SIZE),
                                  &frame, &flags));
        CHECK((flags & (VMM_USER | VMM_WRITABLE | VMM_SHARED)) ==
              (VMM_USER | VMM_WRITABLE | VMM_SHARED));
        CHECK(!(flags & VMM_UNCACHED));
    }
    uint8_t* pixels = (uint8_t*)VGA_PHYSICAL_START;
    for (unsigned frame = 0; frame < 4; ++frame) {
        for (unsigned i = 0; i < 320 * 240; ++i)
            pixels[i] = (i * 13 + i / 320 + frame * 37) & 255;
        vga_swap();
        outb(0x3d4, 0x0c);
        unsigned offset = (unsigned)inb(0x3d5) << 8;
        outb(0x3d4, 0x0d);
        offset |= inb(0x3d5);
        CHECK(offset == ((frame + 1) & 1) * 19200);
        check_visible(pixels);
    }
    static const int rectangles[][4] = {
        {1, 0, 1, 0},         {318, 238, 319, 239}, {2, 2, 4, 3},
        {118, 96, 201, 143},  {0, 3, 319, 10},      {7, 13, 10, 15},
        {300, 200, 319, 220}, {5, 7, 16, 20},       {0, 0, 0, 239}};
    for (unsigned pass = 0; pass < 3; ++pass) {
        for (unsigned r = 0; r < sizeof(rectangles) / sizeof(rectangles[0]);
             ++r) {
            const int* d = rectangles[r];
            for (int y = d[1]; y <= d[3]; ++y)
                for (int x = d[0]; x <= d[2]; ++x)
                    pixels[y * 320 + x] =
                        (x * 13 + y * 17 + r * 23 + pass) & 255;
            vga_swap_rect(d[0], d[1], d[2], d[3]);
            check_visible(pixels);
        }
    }
    vga_swap_rect(-10, -10, -1, -1);
    vga_swap_rect(10, 10, 9, 9);
    vga_swap_rect(320, 240, 500, 500);
    check_visible(pixels);
    pixels[0] = 131;
    vga_swap_rect(-10, -10, 0, 0);
    check_visible(pixels);
    vga_swap();
    check_visible(pixels);

    CHECK(vga_unlock_screen());
    CHECK(!vmm_is_valid_userptr((void*)VGA_PHYSICAL_START, 320 * 240, true));
    CHECK(!vga_unlock_screen());
    CHECK(vga_lock_screen());
    for (unsigned i = 0; i < 320 * 240; ++i)
        CHECK(pixels[i] == 0);
    CHECK(vga_unlock_screen());
    trace("VGA packing, partial damage, page history, mapping and relock "
          "checks passed");
}
#else
#    define _GNU_SOURCE
#    include "../../user/lib/rasta.h"
#    include <limits.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#    include <sys/mman.h>
#    undef trace
#    define trace(...)                                                        \
        do {                                                                  \
            printf(__VA_ARGS__);                                              \
            putchar('\n');                                                    \
        } while (0)
#    define CHECK(cond)                                                       \
        do {                                                                  \
            if (!(cond)) {                                                    \
                fprintf(stderr, "Failed line %d: %s\n", __LINE__, #cond);     \
                abort();                                                      \
            }                                                                 \
        } while (0)
#    define WIDTH              VGA_MODE_X_WIDTH
#    define HEIGHT             VGA_MODE_X_HEIGHT

static struct Bitmap* vga_load_bitmap(const void* buffer, size_t len,
                                      int width, int height)
{
    if (width <= 0 || height <= 0 || (size_t)width > len / height)
        return NULL;
    struct Bitmap* bmp = malloc(sizeof(*bmp) + (size_t)width * height);
    CHECK(bmp);
    *bmp = (struct Bitmap) {width, height, (uint8_t*)(bmp + 1)};
    memcpy(bmp->pixels, buffer, (size_t)width * height);
    return bmp;
}
#    define vga_destroy_bitmap free

static uint8_t expected[WIDTH * HEIGHT];
static uint8_t snapshot[WIDTH * HEIGHT];

static void read_screen(uint8_t* pixels)
{
    memcpy(pixels, (void*)VGA_PHYSICAL_START, WIDTH * HEIGHT);
}

static void check_screen(void)
{
    read_screen(snapshot);
    CHECK(memcmp(snapshot, expected, sizeof(expected)) == 0);
}

static void reference_pixel(int x, int y, uint8_t color)
{
    if ((unsigned)x < WIDTH && (unsigned)y < HEIGHT)
        expected[y * WIDTH + x] = color;
}

static void reference_line(int x1, int y1, int x2, int y2, uint8_t color)
{
    int dx = x2 > x1 ? x2 - x1 : x1 - x2;
    int dy = y2 > y1 ? y2 - y1 : y1 - y2;
    if ((dx >= dy && x1 > x2) || (dx < dy && y1 > y2)) {
        int tmp = x1;
        x1 = x2;
        x2 = tmp;
        tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int error = dx - dy;
    for (;;) {
        reference_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2)
            break;
        int twice = 2 * error;
        if (twice >= -dy) {
            error -= dy;
            x1 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y1 += sy;
        }
    }
}

static void reference_bounds(int* a, int* b)
{
    if (*a > *b) {
        int tmp = *a;
        *a = *b;
        *b = tmp;
    }
}

static void reference_hspan(int x1, int y, int x2, uint8_t color)
{
    if ((unsigned)y >= HEIGHT)
        return;
    if (x1 < 0)
        x1 = 0;
    if (x2 >= WIDTH)
        x2 = WIDTH - 1;
    for (int x = x1; x <= x2; ++x)
        reference_pixel(x, y, color);
}

static void reference_vspan(int x, int y1, int y2, uint8_t color)
{
    if ((unsigned)x >= WIDTH)
        return;
    if (y1 < 0)
        y1 = 0;
    if (y2 >= HEIGHT)
        y2 = HEIGHT - 1;
    for (int y = y1; y <= y2; ++y)
        reference_pixel(x, y, color);
}

static void reference_fillrect(int x1, int y1, int x2, int y2, uint8_t color)
{
    reference_bounds(&x1, &x2);
    reference_bounds(&y1, &y2);
    if (y1 < 0)
        y1 = 0;
    if (y2 >= HEIGHT)
        y2 = HEIGHT - 1;
    for (int y = y1; y <= y2; ++y)
        reference_hspan(x1, y, x2, color);
}

static void reference_rect(int x1, int y1, int x2, int y2, uint8_t color)
{
    reference_bounds(&x1, &x2);
    reference_bounds(&y1, &y2);
    reference_hspan(x1, y1, x2, color);
    reference_hspan(x1, y2, x2, color);
    reference_vspan(x1, y1, y2, color);
    reference_vspan(x2, y1, y2, color);
}

static void reference_oval(int x1, int y1, int x2, int y2, uint8_t color)
{
    reference_bounds(&x1, &x2);
    reference_bounds(&y1, &y2);
    if (x2 < 0 || y2 < 0 || x1 >= VGA_MODE_X_WIDTH || y1 >= VGA_MODE_X_HEIGHT)
        return;
    if (x1 == x2 || y1 == y2) {
        reference_fillrect(x1, y1, x2, y2, color);
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
        reference_pixel(x2, y1, color);
        reference_pixel(x1, y1, color);
        reference_pixel(x1, y2, color);
        reference_pixel(x2, y2, color);
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
        reference_pixel(x1 - 1, y1, color);
        reference_pixel(x2 + 1, y1++, color);
        reference_pixel(x1 - 1, y2, color);
        reference_pixel(x2 + 1, y2--, color);
    }
}

static void reference_roundrect(int x1, int y1, int x2, int y2, int radius,
                                uint8_t color)
{
    reference_bounds(&x1, &x2);
    reference_bounds(&y1, &y2);
    if (x2 < 0 || y2 < 0 || x1 >= VGA_MODE_X_WIDTH || y1 >= VGA_MODE_X_HEIGHT)
        return;
    int64_t max_radius = ((int64_t)x2 - x1) / 2;
    if (((int64_t)y2 - y1) / 2 < max_radius)
        max_radius = ((int64_t)y2 - y1) / 2;
    if (radius > max_radius)
        radius = max_radius;
    if (radius <= 0) {
        reference_rect(x1, y1, x2, y2, color);
        return;
    }

    int left = x1 + radius;
    int right = x2 - radius;
    int top = y1 + radius;
    int bottom = y2 - radius;
    reference_hspan(left, y1, right, color);
    reference_hspan(left, y2, right, color);
    reference_vspan(x1, top, bottom, color);
    reference_vspan(x2, top, bottom, color);
    int x = radius;
    int y = 0;
    int64_t error = 1 - radius;
    while (x >= y) {
        reference_pixel(left - x, top - y, color);
        reference_pixel(left - y, top - x, color);
        reference_pixel(right + x, top - y, color);
        reference_pixel(right + y, top - x, color);
        reference_pixel(left - x, bottom + y, color);
        reference_pixel(left - y, bottom + x, color);
        reference_pixel(right + x, bottom + y, color);
        reference_pixel(right + y, bottom + x, color);
        ++y;
        if (error < 0) {
            error += 2 * (int64_t)y + 1;
        } else {
            --x;
            error += 2 * ((int64_t)y - x) + 1;
        }
    }
}

static void reference_blit(const uint8_t* src, int sw, int sh, int sx, int sy,
                           uint8_t* dst, int dw, int dh, int dx, int dy,
                           int width, int height)
{
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if ((unsigned)(sx + x) < (unsigned)sw &&
                (unsigned)(sy + y) < (unsigned)sh &&
                (unsigned)(dx + x) < (unsigned)dw &&
                (unsigned)(dy + y) < (unsigned)dh)
                dst[(dy + y) * dw + dx + x] = src[(sy + y) * sw + sx + x];
}

static void check_spans(void)
{
    trace("VGA: checking spans, row fills and clipping");
    for (int phase = 0; phase < 4; ++phase) {
        vga_clear(0x53);
        memset(expected, 0x53, sizeof(expected));
        for (int i = 1; i <= 80; ++i) {
            int x1 = phase * 5;
            int x2 = x1 + i * 4 + phase - 4;
            int y1 = (i - 1) * 3;
            int y2 = y1 + 2;
            uint8_t color = i * 7;
            if (i & 1)
                vga_fillrect(x2, y2, x1, y1, color);
            else
                vga_fillrect(x1, y1, x2, y2, color);
            for (int y = y1; y <= y2; ++y)
                for (int x = x1; x <= x2; ++x)
                    reference_pixel(x, y, color);
        }
        check_screen();
    }

    vga_clear(0);
    memset(expected, 0, sizeof(expected));
    vga_fillrect(-10, 91, WIDTH + 10, 97, 0x53);
    for (int y = 91; y <= 97; ++y)
        for (int x = 0; x < WIDTH; ++x)
            reference_pixel(x, y, 0x53);
    for (int i = 0; i < 16; ++i) {
        int x = i * 23 - 12;
        int y = i * 17 - 8;
        vga_drawline(WIDTH + 20, y, -20, y, i + 1);
        reference_line(WIDTH + 20, y, -20, y, i + 1);
        vga_drawline(x, HEIGHT + 20, x, -20, i + 32);
        reference_line(x, HEIGHT + 20, x, -20, i + 32);
        vga_rect(x, HEIGHT + 10, x, -10, i + 64);
        reference_line(x, HEIGHT + 10, x, -10, i + 64);
        vga_rect(WIDTH + 10, y, -10, y, i + 96);
        reference_line(WIDTH + 10, y, -10, y, i + 96);
    }
    vga_drawline(INT_MIN, 1, INT_MAX, 1, 255);
    reference_line(0, 1, WIDTH - 1, 1, 255);
    vga_drawline(1, INT_MAX, 1, INT_MIN, 255);
    reference_line(1, 0, 1, HEIGHT - 1, 255);
    vga_rect(INT_MIN, INT_MIN, INT_MAX, INT_MAX, 255);
    check_screen();
}

static void check_primitives(void)
{
    trace("VGA: checking fills, outlines, line octants and clipping");
    vga_clear(0x53);
    memset(expected, 0x53, sizeof(expected));
    check_screen();
    vga_clear(0);
    memset(expected, 0, sizeof(expected));

    for (int i = 0; i < 16; ++i) {
        int left = i * 21 - 8;
        int top = i * 15 - 4;
        vga_fillrect(left + 13, top + 8, left, top, i + 1);
        for (int y = top; y <= top + 8; ++y)
            for (int x = left; x <= left + 13; ++x)
                reference_pixel(x, y, i + 1);
        vga_rect(left - 1, top - 1, left + 14, top + 9, 255);
        reference_line(left - 1, top - 1, left + 14, top - 1, 255);
        reference_line(left - 1, top + 9, left + 14, top + 9, 255);
        reference_line(left - 1, top - 1, left - 1, top + 9, 255);
        reference_line(left + 14, top - 1, left + 14, top + 9, 255);
    }
    for (int x = -90; x <= 410; x += 17) {
        vga_drawline(160, 120, x, -40, 0xe0);
        reference_line(160, 120, x, -40, 0xe0);
        vga_drawline(x, 280, 160, 120, 0x1c);
        reference_line(x, 280, 160, 120, 0x1c);
    }
    for (int y = -30; y <= 270; y += 11) {
        vga_drawline(-50, y, 160, 120, 3);
        reference_line(-50, y, 160, 120, 3);
        vga_drawline(160, 120, 370, y, 0xfc);
        reference_line(160, 120, 370, y, 0xfc);
    }
    vga_drawline(INT_MIN, INT_MIN, INT_MAX, INT_MAX, 255);
    reference_line(0, 0, 239, 239, 255);
    vga_drawline(7, 5, 7, 5, 0x81);
    reference_pixel(7, 5, 0x81);
    vga_fillrect(-20, -20, -1, -1, 1);
    vga_rect(-10, -10, WIDTH + 10, HEIGHT + 10, 1);
    check_screen();
}

static void check_curves(void)
{
    trace("VGA: checking curve symmetry, bounds, degeneracy and clipping");
    for (int kind = 0; kind < 2; ++kind) {
        vga_clear(0);
        for (int h = 0; h < 12; ++h) {
            for (int w = 0; w < 12; ++w) {
                int x = 20 + w * 20;
                int y = 20 + h * 16;
                if (kind == 0)
                    vga_oval(x, y, x + w, y + h, 255);
                else
                    vga_roundrect(x + w, y + h, x, y, 20, 255);
            }
        }
        read_screen(snapshot);
        for (int h = 0; h < 12; ++h) {
            for (int w = 0; w < 12; ++w) {
                int left = 20 + w * 20;
                int top = 20 + h * 16;
                unsigned edges = 0;
                for (int y = -1; y <= h + 1; ++y) {
                    for (int x = -1; x <= w + 1; ++x) {
                        uint8_t value = snapshot[(top + y) * WIDTH + left + x];
                        if (x < 0 || y < 0 || x > w || y > h) {
                            CHECK(value == 0);
                            continue;
                        }
                        CHECK(value ==
                              snapshot[(top + h - y) * WIDTH + left + x]);
                        CHECK(value ==
                              snapshot[(top + y) * WIDTH + left + w - x]);
                        if (value) {
                            edges |= x == 0 ? 1 : 0;
                            edges |= x == w ? 2 : 0;
                            edges |= y == 0 ? 4 : 0;
                            edges |= y == h ? 8 : 0;
                        }
                    }
                }
                if (edges != 15)
                    trace("Curve bounds: kind=%d width=%d height=%d edges=%u",
                          kind, w, h, edges);
                CHECK(edges == 15);
            }
        }
    }

    vga_clear(0);
    vga_oval(100, 80, 170, 124, 0xe3);
    vga_roundrect(180, 80, 250, 124, 12, 0xfc);
    read_screen(snapshot);
    memset(expected, 0, sizeof(expected));
    reference_blit(snapshot, WIDTH, HEIGHT, 100, 80, expected, WIDTH, HEIGHT,
                   -21, -13, 71, 45);
    reference_blit(snapshot, WIDTH, HEIGHT, 180, 80, expected, WIDTH, HEIGHT,
                   290, 220, 71, 45);
    vga_clear(0);
    vga_oval(-21, -13, 49, 31, 0xe3);
    vga_roundrect(290, 220, 360, 264, 12, 0xfc);
    check_screen();
    vga_clear(0);
    memset(expected, 0, sizeof(expected));
    vga_roundrect(5, 6, 27, 19, 0, 255);
    reference_line(5, 6, 27, 6, 255);
    reference_line(5, 19, 27, 19, 255);
    reference_line(5, 6, 5, 19, 255);
    reference_line(27, 6, 27, 19, 255);
    check_screen();
}

static void check_arithmetic_paths(void)
{
    static const int lines[][4] = {{0, 0, 319, 127},
                                   {319, 127, 0, 0},
                                   {0, 0, 319, 239},
                                   {319, 239, 0, 0},
                                   {319, 0, 0, 239},
                                   {0, 239, 319, 0},
                                   {319, 0, 300, 239},
                                   {300, 239, 319, 0},
                                   {0, 0, 2, 239},
                                   {2, 239, 0, 0},
                                   {1, 1, 318, 238},
                                   {318, 1, 1, 238},
                                   {1, 239, 318, 238},
                                   {1, 238, 318, 239},
                                   {-32768, -32768, 32767, 32767},
                                   {-32769, -32769, 32768, 32768},
                                   {-32768, 239, 32767, 0},
                                   {-32768, 100, 32767, 101},
                                   {100, 32767, 101, -32768},
                                   {100, 32768, 101, -32769},
                                   {-32768, 0, -32767, 239},
                                   {32767, 0, 32766, 239}};
    static const int ovals[][4] = {
        {0, 0, 319, 239},     {-10, -20, 310, 220},       {-32, -40, 352, 344},
        {-33, -41, 352, 344}, {0, 0, 384, 384},           {0, 0, 385, 385},
        {350, 340, -33, -44}, {-1, 120, 383, 121},        {159, -1, 160, 383},
        {100, 20, 485, 239},  {-1024, -1024, 1024, 1024}, {1, 1, 318, 238},
        {0, 0, 1, 239},       {318, 0, 319, 239},         {0, 0, 319, 1},
        {0, 238, 319, 239},   {2, 3, 317, 236},           {3, 2, 316, 237},
        {0, 0, 2, 2},         {317, 237, 319, 239},       {0, 0, 3, 1}};
    static const int rounded[][5] = {{10, 20, 200, 100, 20},
                                     {90, 80, 5, 3, INT_MAX},
                                     {-30, -40, 400, 280, 79},
                                     {0, 0, 319, 239, -1},
                                     {-32767, 0, 32767, 65534, 32767},
                                     {-32768, 0, 32768, 65536, 32768},
                                     {0, 0, 319, 239, INT_MAX},
                                     {1, 1, 318, 238, INT_MAX},
                                     {2, 3, 317, 236, 17},
                                     {3, 2, 316, 237, 1},
                                     {0, 0, 1, 239, 200},
                                     {318, 0, 319, 239, 1},
                                     {0, 0, 319, 0, 8},
                                     {4, 4, 6, 6, 1}};

    trace("VGA: comparing visible, clipped and wide raster paths");
    for (unsigned i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i) {
        const int* p = lines[i];
        vga_clear(0);
        memset(expected, 0, sizeof(expected));
        vga_drawline(p[0], p[1], p[2], p[3], 255);
        reference_line(p[0], p[1], p[2], p[3], 255);
        check_screen();
    }
    for (unsigned i = 0; i < sizeof(ovals) / sizeof(ovals[0]); ++i) {
        const int* p = ovals[i];
        vga_clear(0);
        memset(expected, 0, sizeof(expected));
        vga_oval(p[0], p[1], p[2], p[3], 255);
        reference_oval(p[0], p[1], p[2], p[3], 255);
        check_screen();
    }
    for (unsigned i = 0; i < sizeof(rounded) / sizeof(rounded[0]); ++i) {
        const int* p = rounded[i];
        vga_clear(0);
        memset(expected, 0, sizeof(expected));
        vga_roundrect(p[0], p[1], p[2], p[3], p[4], 255);
        reference_roundrect(p[0], p[1], p[2], p[3], p[4], 255);
        check_screen();
    }
}

static void check_blits(struct Bitmap* logo, const uint8_t* raw)
{
    trace("VGA: checking bitmap conversion, plane alignment and overlap");
    vga_blit(logo, 0, 0, NULL, 0, 0, WIDTH, HEIGHT);
    memcpy(expected, raw, sizeof(expected));
    check_screen();
    uint8_t source[13 * 9];
    uint8_t target[17 * 11];
    for (unsigned i = 0; i < sizeof(source); ++i)
        source[i] = i * 7 + 1;
    CHECK(vga_load_bitmap(source, sizeof(source) - 1, 13, 9) == NULL);
    struct Bitmap* small = vga_load_bitmap(source, sizeof(source), 13, 9);
    CHECK(small != NULL);
    for (int sx = -4; sx < 8; ++sx) {
        for (int dx = -4; dx < 8; ++dx) {
            memset(target, 0x53, sizeof(target));
            struct Bitmap* dest =
                vga_load_bitmap(target, sizeof(target), 17, 11);
            CHECK(dest != NULL);
            int sy = sx % 3;
            int dy = dx % 3;
            int width = 1 + (sx + 4) * 2;
            vga_blit(small, sx, sy, dest, dx, dy, width, 12);
            reference_blit(source, 13, 9, sx, sy, target, 17, 11, dx, dy,
                           width, 12);
            vga_clear(0);
            memset(expected, 0, sizeof(expected));
            vga_blit(dest, 0, 0, NULL, 2, 3, 17, 11);
            reference_blit(target, 17, 11, 0, 0, expected, WIDTH, HEIGHT, 2, 3,
                           17, 11);
            check_screen();
            vga_destroy_bitmap(dest);
        }
    }
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -5; dx <= 5; ++dx) {
            vga_blit(logo, 0, 0, NULL, 0, 0, WIDTH, HEIGHT);
            memcpy(expected, raw, sizeof(expected));
            reference_blit(raw, WIDTH, HEIGHT, 8, 8, expected, WIDTH, HEIGHT,
                           8 + dx, 8 + dy, 40, 24);
            vga_blit(NULL, 8, 8, NULL, 8 + dx, 8 + dy, 40, 24);
            check_screen();
        }
    }
    vga_blit(NULL, 3, 4, small, 0, 0, 13, 9);
    memcpy(snapshot, expected, sizeof(expected));
    memset(expected, 0, sizeof(expected));
    reference_blit(snapshot, WIDTH, HEIGHT, 3, 4, expected, WIDTH, HEIGHT,
                   WIDTH - 7, HEIGHT - 5, 13, 9);
    vga_clear(0);
    vga_blit(small, 0, 0, NULL, WIDTH - 7, HEIGHT - 5, 13, 9);
    vga_blit(small, INT_MIN, 0, NULL, INT_MAX, 0, INT_MAX, 9);
    vga_blit(small, 0, 0, NULL, 0, 0, 0, 9);
    check_screen();
    vga_destroy_bitmap(small);
}
int main(void)
{
    CHECK(mmap((void*)VGA_PHYSICAL_START, WIDTH * HEIGHT,
               PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
               -1, 0) == (void*)VGA_PHYSICAL_START);
    uint8_t raw[WIDTH * HEIGHT];
    for (unsigned i = 0; i < sizeof(raw); ++i)
        raw[i] = i * 13 + i / WIDTH;
    struct Bitmap* bmp = vga_load_bitmap(raw, sizeof(raw), WIDTH, HEIGHT);
    check_spans();
    check_primitives();
    check_curves();
    check_arithmetic_paths();
    check_blits(bmp, raw);
    free(bmp);
    puts("Userspace VGA pixel checks passed");
    return 0;
}
#endif
