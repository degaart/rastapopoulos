#include "../../kernel/scancode.h"
#include "lcd.h"
#include "ui.h"
#include <rasta.h>
#include <stdio.h>
#include <string.h>

#ifndef SNEK_BENCHMARK
#    define SNEK_BENCHMARK 0
#endif

#ifndef SNEK_BENCHMARK_LEVEL
#    define SNEK_BENCHMARK_LEVEL 1
#endif

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

static void blit_lcd(Lcd* lcd, const struct LcdDamage* dirty, int zoom)
{
    static uint8_t scaled[LCD_WIDTH * LCD_HEIGHT * 9];
    struct Bitmap bmp = {LCD_WIDTH, LCD_HEIGHT, lcd->pixels};
    if (zoom > 1) {
        bmp.width *= zoom;
        bmp.height *= zoom;
        bmp.pixels = scaled;
        for (int y = dirty->y1; y <= dirty->y2; ++y) {
            uint8_t* row = scaled + y * zoom * bmp.width;
            for (int x = dirty->x1; x <= dirty->x2; ++x)
                memset(row + x * zoom, lcd->pixels[y * LCD_WIDTH + x], zoom);
            for (int dy = 1; dy < zoom; ++dy)
                memcpy(row + dy * bmp.width + dirty->x1 * zoom,
                       row + dirty->x1 * zoom,
                       (dirty->x2 - dirty->x1 + 1) * zoom);
        }
    }
    vga_blit(&bmp, dirty->x1 * zoom, dirty->y1 * zoom, NULL,
             (SCREEN_WIDTH - bmp.width) / 2 + dirty->x1 * zoom,
             (SCREEN_HEIGHT - bmp.height) / 2 + dirty->y1 * zoom,
             (dirty->x2 - dirty->x1 + 1) * zoom,
             (dirty->y2 - dirty->y1 + 1) * zoom);
}

static bool translate_key(SnakeUI* ui, uint16_t key)
{
    switch (key) {
    case SCANCODE_UP:
        ui_action(ui, UI_UP);
        break;
    case SCANCODE_DOWN:
        ui_action(ui, UI_DOWN);
        break;
    case SCANCODE_LEFT:
        ui_action(ui, UI_LEFT);
        break;
    case SCANCODE_RIGHT:
        ui_action(ui, UI_RIGHT);
        break;
    case SCANCODE_ENTER:
    case SCANCODE_SPACE:
    case SCANCODE_KP_ENTER:
        ui_action(ui, UI_SELECT);
        break;
    case SCANCODE_ESC:
        ui_action(ui, UI_BACK);
        break;
    case SCANCODE_KP_1:
        ui_keypad(ui, 1);
        break;
    case SCANCODE_KP_2:
        ui_keypad(ui, 2);
        break;
    case SCANCODE_KP_3:
        ui_keypad(ui, 3);
        break;
    case SCANCODE_KP_4:
        ui_keypad(ui, 4);
        break;
    case SCANCODE_KP_6:
        ui_keypad(ui, 6);
        break;
    case SCANCODE_KP_7:
        ui_keypad(ui, 7);
        break;
    case SCANCODE_KP_8:
        ui_keypad(ui, 8);
        break;
    case SCANCODE_KP_9:
        ui_keypad(ui, 9);
        break;
    default:
        return false;
    }
    return true;
}

int main(void)
{
    int ret = sys_lock_screen();
    if (ret < 0) {
        trace("Failed to lock screen");
        return 1;
    }

    sys_vga_set_palette(1, 255, 255, 255);

    static SnakeUI ui;
    ui_init(&ui, SNEK_BENCHMARK ? 1 : (uint32_t)(sys_getticks() % 0xfff1u));

    if (SNEK_BENCHMARK)
        ui.level = SNEK_BENCHMARK_LEVEL;

    static Lcd lcd;
    static Lcd displayed;
    unsigned displayed_zoom = 0;
    unsigned rendered_revision = 0;
    unsigned presents = 0, idle_loops = 0;
    uint32_t idle_clocks = 0;
    bool running = true;
    unsigned frames = 0;
    uint32_t previous = sys_perf_clock();
    uint32_t start = previous;
    uint32_t clock_remainder = 0;
    uint32_t update_clocks = 0, render_clocks = 0, blit_clocks = 0,
             swap_clocks = 0;
    uint32_t max_clocks = 0;
    unsigned game_loops = 0;
    unsigned simulated = 0;
    while (running) {
        uint32_t frame_start = sys_perf_clock();
        uint32_t elapsed =
            (uint64_t)(frame_start - start) * 100 / PERF_CLOCK_HZ;
        if (SNEK_BENCHMARK && simulated < 5 &&
            elapsed >= (simulated < 2 ? simulated * 10
                                      : 1000 + (simulated - 2) * 10)) {
            uint16_t key = simulated < 2 ? SCANCODE_ENTER : SCANCODE_ESC;
            if (ui.screen == UI_TITLE && key == SCANCODE_ESC)
                running = false;
            else
                translate_key(&ui, key);
            ++simulated;
        }
        if (!running)
            break;
        if (ui.screen == UI_PLAY)
            ++game_loops;

        struct KeyEvent evt;
        int overflow;
        while (sys_get_kbd_event(&evt, &overflow, false) == 0) {
            if (evt.type == KEY_PRESSED) {
                if (ui.screen == UI_TITLE) {
                    if (evt.scancode == SCANCODE_ESC) {
                        running = false;
                    } else {
                        ui_action(&ui, UI_SELECT);
                    }
                } else {
                    translate_key(&ui, evt.scancode);
                }
            }
        }

        uint32_t now = sys_perf_clock();
        uint64_t scaled = (uint64_t)(now - previous) * 1000 + clock_remainder;
        ui_update(&ui, scaled / PERF_CLOCK_HZ);
        clock_remainder = scaled % PERF_CLOCK_HZ;
        previous = now;
        uint32_t render_start = sys_perf_clock();
        update_clocks += render_start - now;
        if (SNEK_BENCHMARK != 1 && rendered_revision == ui.revision) {
            sys_sleep(1);
            idle_clocks += (uint32_t)sys_perf_clock() - render_start;
            ++idle_loops;
            continue;
        }
        rendered_revision = ui.revision;
        lcd_ui(&lcd, &ui);

        uint32_t blit_start = sys_perf_clock();
        render_clocks += blit_start - render_start;
        struct LcdDamage dirty = {0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1};
        bool changed =
            SNEK_BENCHMARK == 1 || lcd_update(&displayed, &lcd, &dirty);
        bool zoom_changed = displayed_zoom != ui.zoom;
        if (zoom_changed) {
            vga_clear(0);
            dirty = (struct LcdDamage) {0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1};
            displayed_zoom = ui.zoom;
            changed = true;
        }
        if (changed)
            blit_lcd(&lcd, &dirty, ui.zoom);
        uint32_t swap_start = sys_perf_clock();
        blit_clocks += swap_start - blit_start;
        if (changed) {
            int x = (SCREEN_WIDTH - LCD_WIDTH * (int)ui.zoom) / 2;
            int y = (SCREEN_HEIGHT - LCD_HEIGHT * (int)ui.zoom) / 2;
            if (zoom_changed)
                sys_vga_swap();
            else
                sys_vga_swap_rect(x + dirty.x1 * ui.zoom,
                                  y + dirty.y1 * ui.zoom,
                                  x + (dirty.x2 + 1) * ui.zoom - 1,
                                  y + (dirty.y2 + 1) * ui.zoom - 1);
            ++presents;
        }
        uint32_t done = sys_perf_clock();
        swap_clocks += done - swap_start;
        if (done - frame_start > max_clocks)
            max_clocks = done - frame_start;
        ++frames;
    }

    uint32_t duration = (uint32_t)sys_perf_clock() - start;
    if (duration && frames) {
        trace("SNEK perf: frames=%u clocks=%u fps_x100=%u game_loops=%u",
              frames, (unsigned)duration,
              (unsigned)((uint64_t)frames * PERF_CLOCK_HZ * 100 / duration),
              game_loops);
        trace("SNEK stages: update=%u lcd=%u blit=%u swap=%u max=%u clocks",
              (unsigned)update_clocks, (unsigned)render_clocks,
              (unsigned)blit_clocks, (unsigned)swap_clocks,
              (unsigned)max_clocks);
        trace("SNEK work: presents=%u idle_loops=%u idle_clocks=%u "
              "game_steps=%lu",
              presents, idle_loops, (unsigned)idle_clocks, ui.game.tick);
    }
    if (sys_unlock_screen() < 0) {
        trace("sys_unlock_screen failed");
        return 1;
    }
    return 0;
}
