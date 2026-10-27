#include "lcd.h"
#include "firmware_assets.h"
#include "firmware_ui.h"
#include "nokia_font.h"

#include <stdio.h>
#include <string.h>

void lcd_clear(Lcd* lcd, uint8_t color)
{
    memset(lcd->pixels, color, sizeof(lcd->pixels));
}
void lcd_pixel(Lcd* lcd, int x, int y, uint8_t color)
{
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT)
        lcd->pixels[y * LCD_WIDTH + x] = color;
}
static void rectangle(Lcd* lcd, int x, int y, int width, int height,
                      uint8_t color)
{
    if (width <= 0 || height <= 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT)
        return;
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int64_t right = (int64_t)x + width;
    int64_t bottom = (int64_t)y + height;
    if (right <= 0 || bottom <= 0)
        return;
    int x2 = right > LCD_WIDTH ? LCD_WIDTH : right;
    int y2 = bottom > LCD_HEIGHT ? LCD_HEIGHT : bottom;
    if (x1 >= x2 || y1 >= y2)
        return;
    for (int row = y1; row < y2; ++row)
        memset(lcd->pixels + row * LCD_WIDTH + x1, color, x2 - x1);
}
void lcd_bitmap(Lcd* lcd, int x, int y, const FirmwareBitmap* bitmap)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT || x <= -(int)bitmap->width ||
        y <= -(int)bitmap->height)
        return;
    int left = x < 0 ? -x : 0;
    int top = y < 0 ? -y : 0;
    int right = bitmap->width;
    int bottom = bitmap->height;
    if ((int64_t)x + right > LCD_WIDTH)
        right = LCD_WIDTH - x;
    if ((int64_t)y + bottom > LCD_HEIGHT)
        bottom = LCD_HEIGHT - y;
    if (left >= right || top >= bottom)
        return;
    for (int dy = top; dy < bottom; ++dy) {
        const uint8_t* in = bitmap->columns + (dy >> 3) * bitmap->width + left;
        uint8_t* out = lcd->pixels + (y + dy) * LCD_WIDTH + x + left;
        unsigned shift = dy & 7;
        for (int dx = 0; dx < right - left; ++dx)
            out[dx] = (in[dx] >> shift) & 1;
    }
}
void lcd_text_font(Lcd* lcd, int x, int y, const char* text, NokiaFont font,
                   uint8_t color)
{
    for (; *text; ++text) {
        const NokiaGlyph* glyph = nokia_glyph(font, (unsigned char)*text);
        for (unsigned row = 0; row < glyph->height; ++row)
            for (unsigned col = 0; col < glyph->width; ++col)
                if (glyph->rows[row] & (1u << col))
                    lcd_pixel(lcd, x + (int)col, y + (int)row, color);
        x += glyph->width;
    }
}
void lcd_text(Lcd* lcd, int x, int y, const char* text, uint8_t color)
{
    lcd_text_font(lcd, x, y, text, NOKIA_SMALL_PLAIN, color);
}
static void centered(Lcd* lcd, int y, const char* text, NokiaFont font)
{
    lcd_text_font(lcd, (LCD_WIDTH - nokia_text_width(font, text)) / 2, y, text,
                  font, 1);
}
static void hud_number(Lcd* lcd, int x, uint32_t number, unsigned digits)
{
    while (digits) {
        --digits;
        lcd_bitmap(lcd, x + (int)digits * 4, 0, &firmware_digits[number % 10]);
        number /= 10;
    }
}
static SnakeDirection between(SnakeCell a, SnakeCell b)
{
    if (a.y == b.y)
        return (a.x + 1) % SNAKE_BOARD_WIDTH == b.x ? SNAKE_RIGHT : SNAKE_LEFT;
    return (a.y + 1) % SNAKE_BOARD_HEIGHT == b.y ? SNAKE_DOWN : SNAKE_UP;
}
static int corner(SnakeDirection incoming, SnakeDirection outgoing)
{
    if ((incoming == SNAKE_UP && outgoing == SNAKE_RIGHT) ||
        (incoming == SNAKE_LEFT && outgoing == SNAKE_DOWN))
        return 0;
    if ((incoming == SNAKE_UP && outgoing == SNAKE_LEFT) ||
        (incoming == SNAKE_RIGHT && outgoing == SNAKE_DOWN))
        return 1;
    if ((incoming == SNAKE_LEFT && outgoing == SNAKE_UP) ||
        (incoming == SNAKE_DOWN && outgoing == SNAKE_RIGHT))
        return 2;
    return 3;
}
void lcd_game_visible(Lcd* lcd, const SnakeGame* game, bool snake_visible)
{
    lcd_clear(lcd, 0);
    hud_number(lcd, 0, game->score, 4);
    rectangle(lcd, 0, 6, 84, 1, 1);
    rectangle(lcd, 0, 8, 84, 1, 1);
    rectangle(lcd, 0, 47, 84, 1, 1);
    rectangle(lcd, 0, 8, 1, 40, 1);
    rectangle(lcd, 83, 8, 1, 40, 1);
    if (game->maze)
        for (unsigned y = 0; y < SNAKE_BOARD_HEIGHT; ++y)
            for (unsigned x = 0; x < SNAKE_BOARD_WIDTH; ++x) {
                if (!snake_maze_wall(game->maze, (uint8_t)x, (uint8_t)y))
                    continue;
                int px = 3 + (int)x * 4, py = 11 + (int)y * 4;
                rectangle(lcd, px, py, 2, 2, 1);
                if (x + 1 < SNAKE_BOARD_WIDTH &&
                    snake_maze_wall(game->maze, (uint8_t)(x + 1), (uint8_t)y))
                    rectangle(lcd, px, py, 6, 2, 1);
                if (y + 1 < SNAKE_BOARD_HEIGHT &&
                    snake_maze_wall(game->maze, (uint8_t)x, (uint8_t)(y + 1)))
                    rectangle(lcd, px, py, 2, 6, 1);
            }
    if (game->food_present)
        lcd_bitmap(lcd, 2 + game->food.x * 4, 10 + game->food.y * 4,
                   &firmware_food);
    if (game->bonus_present) {
        const FirmwareBitmap* creature =
            &firmware_creatures[game->bonus_large ? 6 + game->bonus_frame
                                                  : game->bonus_kind % 6];
        lcd_bitmap(lcd, 2 + game->bonus.x * 4, 10 + game->bonus.y * 4,
                   creature);
        hud_number(lcd, 75, game->bonus_ticks, 2);
        lcd_bitmap(
            lcd, 66, 1,
            &firmware_creatures[game->bonus_large ? 8 + game->bonus_frame
                                                  : game->bonus_kind % 6]);
    }
    if (!snake_visible || !game->length)
        return;
    for (unsigned i = 0; i < game->length; ++i) {
        const FirmwareBitmap* sprite;
        if (i + 1u == game->length) {
            sprite =
                &(game->mouth_open ? firmware_open_heads
                                   : firmware_heads)[game->head_direction];
        } else if (i == 0) {
            sprite = &firmware_tails[between(game->body[0], game->body[1])];
        } else {
            SnakeDirection incoming =
                between(game->body[i - 1], game->body[i]);
            SnakeDirection outgoing =
                between(game->body[i], game->body[i + 1]);
            if (incoming == outgoing)
                sprite = &(game->swallowed[i] ? firmware_swallow_bodies
                                              : firmware_bodies)[outgoing];
            else
                sprite = &(game->swallowed[i]
                               ? firmware_swallow_corners
                               : firmware_corners)[corner(incoming, outgoing)];
        }
        lcd_bitmap(lcd, 2 + game->body[i].x * 4, 10 + game->body[i].y * 4,
                   sprite);
    }
}
void lcd_game(Lcd* lcd, const SnakeGame* game, uint32_t top_score)
{
    (void)top_score;
    lcd_game_visible(lcd, game, true);
}
void lcd_message(Lcd* lcd, const char* title, const char* line1,
                 const char* line2)
{
    lcd_clear(lcd, 0);
    centered(lcd, 5, title, NOKIA_SMALL_BOLD);
    centered(lcd, 21, line1, NOKIA_SMALL_PLAIN);
    centered(lcd, 33, line2, NOKIA_SMALL_PLAIN);
}
static void menu_header(Lcd* lcd, const char* title, const char* softkey)
{
    lcd_clear(lcd, 0);
    lcd_text_font(lcd, 1, 0, title, NOKIA_SMALL_PLAIN, 1);
    centered(lcd, 39, softkey, NOKIA_SMALL_BOLD);
}
static void menu_rows(Lcd* lcd, const char* const* labels, unsigned count,
                      unsigned selection)
{
    unsigned first = selection < 3 ? 0 : selection - 2;
    for (unsigned row = 0; row < 3 && first + row < count; ++row) {
        int y = 7 + (int)row * 10;
        bool active = first + row == selection;
        if (active)
            rectangle(lcd, 0, y, 79, 10, 1);
        lcd_text_font(lcd, 2, y + 1, labels[first + row], NOKIA_SMALL_BOLD,
                      active ? 0 : 1);
    }
    rectangle(lcd, 82, 7, 1, 30, 1);
    int top = 7 + (count > 1 ? (int)(selection * 23 / (count - 1)) : 0);
    rectangle(lcd, 81, top, 3, 7, 1);
}
void lcd_ui(Lcd* lcd, const SnakeUI* ui)
{
    char text[32];
    const char* labels[9];
    switch (ui->screen) {
    case UI_TITLE:
    {
        lcd_bitmap(lcd, 0, 0, &firmware_title);
        /* The state counter starts at one; the first visible overlay is
           frame 2. The final counter reads a handle outside the initialized
           frame array; its runtime effect remains unverified. Return to the
           base title. */
        static const int positions[] = {16, 14, 13, 15, 15};
        if (ui->title_step >= 1 && ui->title_step <= 4) {
            unsigned frame = ui->title_step;
            lcd_bitmap(lcd, 6, positions[frame],
                       &firmware_title_frames[frame]);
        }
        break;
    }
    case UI_PLAY:
    case UI_DYING:
        lcd_game_visible(lcd, &ui->game, ui->snake_visible);
        break;
    case UI_MENU:
        menu_header(lcd, "Snake II", "Select");
        for (unsigned i = 0; i < ui_menu_count(ui); ++i)
            labels[i] = ui_menu_item(ui, i);
        menu_rows(lcd, labels, ui_menu_count(ui), ui->selection);
        break;
    case UI_LEVEL:
    {
        menu_header(lcd, "Level", "OK");
        char levels[9][8];
        for (unsigned i = 0; i < 9; ++i) {
            snprintf(levels[i], sizeof(levels[i]), "%u", i + 1);
            labels[i] = levels[i];
        }
        menu_rows(lcd, labels, 9, ui->selection);
        break;
    }
    case UI_MAZE:
    {
        static const char* const mazes[] = {"No maze", "Maze 1", "Maze 2",
                                            "Maze 3",  "Maze 4", "Maze 5"};
        menu_header(lcd, "Mazes", "OK");
        menu_rows(lcd, mazes, 6, ui->selection);
        break;
    }
    case UI_TOP_SCORE:
        menu_header(lcd, "", "OK");
        centered(lcd, 7, "Top score:", NOKIA_SMALL_PLAIN);
        snprintf(text, sizeof(text), "%u", (unsigned)ui->top_score);
        centered(lcd, 22, text, NOKIA_LARGE_BOLD);
        break;
    case UI_INSTRUCTIONS:
    {
        menu_header(lcd, "Instructions", "Back");
        char lines[32][32];
        unsigned count = ui_instruction_lines(lines);
        unsigned first = ui->instruction_line;
        if (count > 3 && first > count - 3)
            first = count - 3;
        for (unsigned row = 0; row < 3 && first + row < count; ++row)
            lcd_text(lcd, 1, 10 + (int)row * 9, lines[first + row], 1);
        break;
    }
    case UI_SOUNDS:
        menu_header(lcd, "Sounds", "OK");
        labels[0] = "On";
        labels[1] = "Off";
        menu_rows(lcd, labels, 2, ui->selection);
        break;
    case UI_ZOOM:
    {
        static const char* const zooms[] = {"x1", "x2", "x3"};
        menu_header(lcd, "Zoom", "OK");
        menu_rows(lcd, zooms, 3, ui->selection);
        break;
    }
    case UI_GAME_OVER:
        menu_header(lcd, "Game over!", "OK");
        centered(lcd, 13, ui->new_record ? "TOP SCORE:" : "Your score:",
                 NOKIA_SMALL_PLAIN);
        snprintf(text, sizeof(text), "%u", (unsigned)ui->game.score);
        centered(lcd, 25, text, NOKIA_LARGE_BOLD);
        break;
    }
}

bool lcd_update(Lcd* displayed, const Lcd* next, struct LcdDamage* damage)
{
    *damage = (struct LcdDamage) {LCD_WIDTH, LCD_HEIGHT, -1, -1};
    for (int y = 0; y < LCD_HEIGHT; ++y) {
        for (int x = 0; x < LCD_WIDTH; x += 4) {
            uint32_t old, value;
            __builtin_memmove(&old, displayed->pixels + y * LCD_WIDTH + x, 4);
            __builtin_memmove(&value, next->pixels + y * LCD_WIDTH + x, 4);
            if (old == value)
                continue;
            __builtin_memcpy(displayed->pixels + y * LCD_WIDTH + x, &value, 4);
            if (x < damage->x1)
                damage->x1 = x;
            if (x + 3 > damage->x2)
                damage->x2 = x + 3;
            if (y < damage->y1)
                damage->y1 = y;
            damage->y2 = y;
        }
    }
    return damage->x1 <= damage->x2;
}
