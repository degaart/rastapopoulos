#include <rasta.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("Locking screen\n");
    int ret = sys_lock_screen();
    if (!ret) {
        trace("Failed to lock screen");
        return 1;
    }

    trace("Screen locked");

    uint8_t pixels[32 * 32];
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x)
            pixels[y * 32 + x] = 32 + ((x / 4 + y / 4) % 2) * 180;
    }
    VGABitmap bitmap = sys_vga_load_bitmap(pixels, sizeof(pixels), 32, 32);
    if (!bitmap)
        trace("Failed to load demo bitmap");

    unsigned frame = 0;
    bool running = true;
    while (running) {
        struct KeyEvent evt;
        int overflow;
        while (sys_get_kbd_event(&evt, &overflow, false) == 0) {
            if (evt.type == KEY_PRESSED)
                running = false;
        }
        if (!running)
            break;

        sys_vga_fillrect(0, 0, 319, 239, 0);
        for (int i = 0; i < 16; ++i) {
            int x = (frame * (i % 3 + 1) + i * 23) % 360 - 40;
            int y = (frame + i * 37) % 280 - 40;
            uint8_t color = 16 + (frame / 3 + i * 13) % 239;
            sys_vga_fillrect(x, y, x + 14, y + 14, color);
            sys_vga_rect(x - 4, y - 4, x + 18, y + 18, color + 1);
            sys_vga_oval(319 - x - 40, y, 319 - x, y + 28, color);
            sys_vga_roundrect(x, 239 - y - 30, x + 48, 239 - y,
                              4 + (frame / 4 + i) % 12, color);
            sys_vga_drawline(160, 120, x, y, color);
        }
        if (bitmap) {
            int x = frame % 576;
            int y = frame % 416;
            if (x > 288)
                x = 576 - x;
            if (y > 208)
                y = 416 - y;
            sys_vga_blit(bitmap, 0, 0, 0, x, y, 32, 32);
            sys_vga_blit(0, x, y, 0, 288 - x, 208 - y, 32, 32);
        }
        sys_vga_rect(0, 0, 319, 239, 255);
        sys_vga_swap();
        ++frame;
        sys_sleep(2);
    }
    if (bitmap)
        sys_vga_destroy_bitmap(bitmap);

    trace("Unlocking screen");
    if (!sys_unlock_screen()) {
        trace("Failed to unlock screen");
        return 1;
    }
    printf("Screen unlocked\n");
    return 0;
}

