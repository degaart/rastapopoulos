#include "../address_space.h"
#include "../kernel.h"
#include "../process.h"
#include "../trace.h"
#include "../vga.h"
#include "../vmm.h"
#include "test.h"

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
