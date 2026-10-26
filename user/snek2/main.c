#include "../../kernel/scancode.h"
#include "lcd.h"
#include "ui.h"
#include <rasta.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

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
    ui_init(&ui, (uint32_t)(sys_getticks() % 0xfff1u));

    static Lcd lcd;
    bool running = true;
    unsigned frames = 0;
    uint32_t previous = sys_getticks();
    while (running) {
        SnakeScreen old_screen = ui.screen;
        uint32_t old_score = ui.game.score;

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

        uint32_t now = sys_getticks();
        ui_update(&ui, (now - previous) * 10ull);
        previous = now;
        lcd_ui(&lcd, &ui);

        VGABitmap bmp = sys_vga_load_bitmap(lcd.pixels, sizeof(lcd.pixels),
                                            LCD_WIDTH, LCD_HEIGHT);
        if (!bmp) {
            if (sys_unlock_screen() < 0) {
                trace("sys_unlock_screen failed");
                return 1;
            }
            printf("sys_vga_load_bitmap failed\n");
            return 1;
        }
        sys_vga_blit(bmp, 0, 0, 0, (SCREEN_WIDTH - LCD_WIDTH) / 2,
                     (SCREEN_HEIGHT - LCD_HEIGHT) / 2, LCD_WIDTH, LCD_HEIGHT);
        sys_vga_swap();
        sys_vga_destroy_bitmap(bmp);
        sys_sleep(1);
    }

    if (sys_unlock_screen() < 0) {
        trace("sys_unlock_screen failed");
        return 1;
    }
    return 0;
}
