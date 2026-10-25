#include "../kernel.h"
#include "../trace.h"
#include "../vga.h"
#include "../vmm.h"

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            trace("Assert failed: %s", #cond);                                \
            debugexit(2);                                                     \
        }                                                                     \
    } while (0)

void test_vga(void)
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

    for (unsigned y = 0; y < VGA_MODE_X_HEIGHT; ++y) {
        for (unsigned x = 0; x < VGA_MODE_X_WIDTH; ++x) {
            uint8_t color = (y / 15) * 16 + x / 20;
            if (x == 0 || x == VGA_MODE_X_WIDTH - 1 || y == 0 ||
                y == VGA_MODE_X_HEIGHT - 1) {
                color = 255;
            }
            vga_putpixel(x, y, color);
        }
    }

    for (int i = 0; i < 100000000; i++) {
        io_wait();
    }

    for (void* ptr = (void*)VGA_PHYSICAL_START; ptr < (void*)VGA_PHYSICAL_END;
         ptr += PAGE_SIZE) {
        if (ptr != (void*)VGA_PHYSICAL_BASE) {
            CHECK(vmm_unmap(ptr));
        }
    }

    vga_text_mode();
}

