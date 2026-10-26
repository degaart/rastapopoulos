#include "../../user/snek2/firmware_ui.h"
#include "../../user/snek2/lcd.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void reference_lcd_bitmap(Lcd* lcd, int x, int y,
                                 const FirmwareBitmap* bitmap)
{
    for (unsigned dy = 0; dy < bitmap->height; ++dy)
        for (unsigned dx = 0; dx < bitmap->width; ++dx)
            if ((unsigned)(x + (int)dx) < LCD_WIDTH &&
                (unsigned)(y + (int)dy) < LCD_HEIGHT)
                lcd->pixels[(y + dy) * LCD_WIDTH + x + dx] =
                    (bitmap->columns[dx + (dy / 8) * bitmap->width] >>
                     (dy % 8)) &
                    1;
}

static uint64_t reference_hash = 14695981039346656037ull;

static void check_state(SnakeUI* ui, Lcd* cached, unsigned* revision)
{
    Lcd actual = {0};
    lcd_ui(&actual, ui);
    for (unsigned i = 0; i < sizeof(actual.pixels); ++i) {
        reference_hash ^= actual.pixels[i];
        reference_hash *= 1099511628211ull;
    }
    if (*revision != ui->revision) {
        *revision = ui->revision;
        *cached = actual;
    }
    assert(memcmp(cached->pixels, actual.pixels, sizeof(actual.pixels)) == 0);
}

int main(void)
{
    Lcd before = {0}, next = {0}, reconstructed = {0};
    struct LcdDamage dirty;
    assert(!lcd_update(&before, &next, &dirty));
    for (unsigned i = 0; i < sizeof(next.pixels); ++i) {
        next.pixels[i] = i * 13 + 1;
        bool changed =
            memcmp(before.pixels, next.pixels, sizeof(next.pixels)) != 0;
        assert(lcd_update(&before, &next, &dirty) == changed);
        if (changed)
            for (int y = dirty.y1; y <= dirty.y2; ++y)
                memcpy(reconstructed.pixels + y * LCD_WIDTH + dirty.x1,
                       next.pixels + y * LCD_WIDTH + dirty.x1,
                       dirty.x2 - dirty.x1 + 1);
        assert(memcmp(before.pixels, next.pixels, sizeof(next.pixels)) == 0);
        assert(memcmp(reconstructed.pixels, next.pixels,
                      sizeof(next.pixels)) == 0);
    }
    for (int y = -55; y <= 55; y += 3) {
        for (int x = -95; x <= 95; x += 3) {
            Lcd expected = {0}, actual = {0};
            reference_lcd_bitmap(&expected, x, y, &firmware_title);
            lcd_bitmap(&actual, x, y, &firmware_title);
            assert(memcmp(expected.pixels, actual.pixels,
                          sizeof(actual.pixels)) == 0);
        }
    }
    lcd_bitmap(&next, INT_MIN, INT_MAX, &firmware_title);
    for (unsigned maze = 0; maze < SNAKE_MAZE_COUNT; ++maze) {
        for (unsigned level = 1; level <= SNAKE_LEVEL_COUNT; ++level) {
            SnakeUI ui;
            ui_init(&ui, 1);
            Lcd cached = {0};
            unsigned revision = 0;
            check_state(&ui, &cached, &revision);
            ui_action(&ui, UI_SELECT);
            ui.maze = maze;
            ui.level = level;
            ui_action(&ui, UI_SELECT);
            for (unsigned t = 0; t < 1600; ++t) {
                if (t % 97 == 0)
                    ui_action(&ui, (UiAction)((t / 97) % 4));
                ui_update(&ui, 10);
                check_state(&ui, &cached, &revision);
            }
        }
    }
    for (unsigned selection = 0; selection < 5; ++selection) {
        SnakeUI ui;
        ui_init(&ui, 7);
        Lcd cached = {0};
        unsigned revision = 0;
        ui_action(&ui, UI_SELECT);
        for (unsigned i = 0; i < selection; ++i)
            ui_action(&ui, UI_DOWN);
        check_state(&ui, &cached, &revision);
        ui_action(&ui, UI_SELECT);
        check_state(&ui, &cached, &revision);
        for (unsigned i = 0; i < 10; ++i) {
            ui_action(&ui, UI_DOWN);
            check_state(&ui, &cached, &revision);
        }
        ui_action(&ui, UI_BACK);
        check_state(&ui, &cached, &revision);
    }
    /* Captured from the pre-optimization renderer over the same state
     * sequence. */
    assert(reference_hash == 0x02051d103052cc63ull);
    puts("Snek LCD pixels, clipping, damage and redraw invalidation checks "
         "passed");
}
