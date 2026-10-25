#include "../kernel.h"
#include "../pit.h"
#include "../ramdisk.h"
#include "../tar.h"
#include "../trace.h"
#include "../vga.h"
#include "../vmm.h"

#ifndef VGA_TEST_CHECKS
#    define VGA_TEST_CHECKS 0
#endif

#if VGA_TEST_CHECKS
#    include <limits.h>
#    include <string.h>
#endif

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            trace("Assert failed: %s", #cond);                                \
            debugexit(2);                                                     \
        }                                                                     \
    } while (0)

static void setup(void)
{
    for (void* ptr = (void*)VGA_PHYSICAL_START; ptr < (void*)VGA_PHYSICAL_END;
         ptr += PAGE_SIZE) {
        if (ptr != (void*)VGA_PHYSICAL_BASE) {
            CHECK(
                vmm_map(ptr, ptr,
                        VMM_USER | VMM_WRITABLE | VMM_UNCACHED | VMM_SHARED));
        }
    }

    vga_mode_x();
}

static void teardown(void)
{
    for (void* ptr = (void*)VGA_PHYSICAL_START; ptr < (void*)VGA_PHYSICAL_END;
         ptr += PAGE_SIZE) {
        if (ptr != (void*)VGA_PHYSICAL_BASE) {
            CHECK(vmm_unmap(ptr));
        }
    }

    vga_text_mode();
}

static void delay(uint64_t ticks)
{
    enable_interrupts();
    uint64_t deadline = pit_get_ticks() + ticks;
    while (pit_get_ticks() < deadline)
        hlt();
    disable_interrupts();
}

#define WIDTH  VGA_MODE_X_WIDTH
#define HEIGHT VGA_MODE_X_HEIGHT

#if VGA_TEST_CHECKS
static uint8_t expected[WIDTH * HEIGHT];
static uint8_t snapshot[WIDTH * HEIGHT];

static void read_screen(uint8_t* pixels)
{
    for (int plane = 0; plane < 4; ++plane) {
        outb(0x3ce, 4);
        outb(0x3cf, plane);
        const volatile uint8_t* src = (const volatile uint8_t*)VGA_START;
        for (int y = 0; y < HEIGHT; ++y)
            for (int x = plane; x < WIDTH; x += 4)
                pixels[y * WIDTH + x] = *src++;
    }
}

static void check_screen(void)
{
    vga_swap();
    read_screen(snapshot);
    for (int i = 0; i < WIDTH * HEIGHT; ++i) {
        if (snapshot[i] != expected[i]) {
            trace("VGA mismatch (%d,%d): got %u, expected %u", i % WIDTH,
                  i / WIDTH, snapshot[i], expected[i]);
            debugexit(2);
        }
    }
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

static void check_primitives(void)
{
    trace("VGA: checking fills, outlines, line octants and clipping");
    vga_clear(0x53);
    memset(expected, 0x53, sizeof(expected));
    check_screen();
    vga_clear(0);
    read_screen(snapshot);
    CHECK(memcmp(snapshot, expected, sizeof(expected)) == 0);
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
        vga_swap();
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
    vga_swap();
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
#endif

static void animate(struct Bitmap* logo)
{
    static const char* names[] = {"fills", "rectangles",         "lines",
                                  "ovals", "rounded rectangles", "blits"};
    for (int scene = 0; scene < 6; ++scene) {
        trace("VGA animation: %s (uncapped, 100 PIT ticks)", names[scene]);
        enable_interrupts();
        uint64_t start = pit_get_ticks();
        uint64_t now = start;
        unsigned frames = 0;
        do {
            unsigned phase = (now - start) * 3;
            vga_clear(0);
            for (int i = 0; i < 48; ++i) {
                int x = (phase + i * 37) % 400 - 40;
                int y = (phase * 2 + i * 23) % 300 - 30;
                uint8_t color = 1 + (i * 19 + phase) % 255;
                switch (scene) {
                case 0:
                    vga_fillrect(x, y, x + 39, y + 27, color);
                    break;
                case 1:
                    vga_rect(x, y, x + 57, y + 39, color);
                    break;
                case 2:
                    vga_drawline(160, 120, x, y, color);
                    break;
                case 3:
                    vga_oval(x, y, x + 57, y + 39, color);
                    break;
                case 4:
                    vga_roundrect(x, y, x + 57, y + 39, 4 + i % 16, color);
                    break;
                case 5:
                    vga_blit(logo, (i * 17) % 240, (i * 13) % 180, NULL, x, y,
                             80, 60);
                    break;
                }
            }
            vga_rect(0, 0, WIDTH - 1, HEIGHT - 1, 255);
            vga_swap();
            ++frames;
            now = pit_get_ticks();
        } while (now - start < 100);
        disable_interrupts();
        trace("VGA %s: %u frames, %llu ticks, %llu FPS (48 primitives/frame)",
              names[scene], frames, now - start,
              (uint64_t)frames * 100 / (now - start));
    }
    vga_blit(logo, 0, 0, NULL, 0, 0, WIDTH, HEIGHT);
    vga_swap();
}

void test_vga(void)
{
    setup();

    struct Tar tar;
    CHECK(tar_open(&tar, ramdisk_start, ramdisk_end - ramdisk_start) != false);

    void* bitmap_buffer;
    size_t bitmap_buffer_len;
    CHECK(tar_open_file(&tar, "logo.raw", &bitmap_buffer,
                        &bitmap_buffer_len) != false);

    struct Bitmap* bmp =
        vga_load_bitmap(bitmap_buffer, bitmap_buffer_len, 320, 240);

    CHECK(bmp != NULL);
#if VGA_TEST_CHECKS
    check_primitives();
    check_curves();
    check_blits(bmp, bitmap_buffer);
    trace("VGA pixel checks passed");
#endif
    animate(bmp);

    vga_destroy_bitmap(bmp);
    delay(100);
    teardown();
    trace("VGA tests passed");
    printf("VGA tests passed\n");
    halt();
}

