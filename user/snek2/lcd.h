#ifndef SNAKE_LCD_H
#define SNAKE_LCD_H

#include "firmware_assets.h"
#include "nokia_font.h"
#include "snake.h"
#include "ui.h"
#include <stdint.h>

enum
{
    LCD_WIDTH = 84,
    LCD_HEIGHT = 48
};
typedef struct
{
    uint8_t pixels[LCD_WIDTH * LCD_HEIGHT];
} Lcd;

void lcd_clear(Lcd* lcd, uint8_t color);
void lcd_pixel(Lcd* lcd, int x, int y, uint8_t color);
void lcd_text(Lcd* lcd, int x, int y, const char* text, uint8_t color);
void lcd_text_font(Lcd* lcd, int x, int y, const char* text, NokiaFont font,
                   uint8_t color);
void lcd_bitmap(Lcd* lcd, int x, int y, const FirmwareBitmap* bitmap);
void lcd_game_visible(Lcd* lcd, const SnakeGame* game, bool snake_visible);
void lcd_ui(Lcd* lcd, const SnakeUI* ui);
void lcd_game(Lcd* lcd, const SnakeGame* game, uint32_t top_score);
void lcd_message(Lcd* lcd, const char* title, const char* line1,
                 const char* line2);

#endif
