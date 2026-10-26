#ifndef SNAKE_UI_H
#define SNAKE_UI_H

#include "snake.h"

typedef enum
{
    UI_TITLE,
    UI_MENU,
    UI_LEVEL,
    UI_MAZE,
    UI_TOP_SCORE,
    UI_INSTRUCTIONS,
    UI_SOUNDS,
    UI_PLAY,
    UI_DYING,
    UI_GAME_OVER
} SnakeScreen;
typedef enum
{
    UI_UP,
    UI_DOWN,
    UI_LEFT,
    UI_RIGHT,
    UI_SELECT,
    UI_BACK
} UiAction;
typedef struct
{
    SnakeScreen screen;
    unsigned revision;
    SnakeGame game;
    unsigned level, maze, top_level;
    uint32_t top_score, seed;
    bool sounds, has_game, new_record;
    unsigned selection, first_visible, instruction_line;
    unsigned title_step;
    uint32_t elapsed_ms, accumulator_ms;
    bool snake_visible;
} SnakeUI;

void ui_init(SnakeUI* ui, uint32_t seed);
void ui_action(SnakeUI* ui, UiAction action);
void ui_keypad(SnakeUI* ui, unsigned key);
void ui_update(SnakeUI* ui, uint32_t elapsed_ms);
unsigned ui_menu_count(const SnakeUI* ui);
const char* ui_menu_item(const SnakeUI* ui, unsigned index);
unsigned ui_instruction_lines(char lines[32][32]);

#endif
