#include "ui.h"
#include "firmware_ui.h"
#include "nokia_font.h"

#include <string.h>

static void enter(SnakeUI* ui, SnakeScreen screen)
{
    ui->screen = screen;
    ++ui->revision;
    ui->elapsed_ms = ui->accumulator_ms = 0;
    ui->selection = ui->first_visible = 0;
    ui->snake_visible = true;
}

void ui_init(SnakeUI* ui, uint32_t seed)
{
    memset(ui, 0, sizeof(*ui));
    ui->level = ui->top_level = 1;
    ui->zoom = 1;
    ui->sounds = true;
    ui->seed = seed;
    enter(ui, UI_TITLE);
}

unsigned ui_menu_count(const SnakeUI* ui)
{
    return ui->has_game ? 7 : 6;
}

const char* ui_menu_item(const SnakeUI* ui, unsigned index)
{
    static const char* const items[] = {"New game",  "Level",        "Mazes",
                                        "Top score", "Instructions", "Zoom"};
    if (ui->has_game) {
        if (index == 0)
            return "Continue";
        --index;
    }
    return index < 6 ? items[index] : "";
}

static void new_game(SnakeUI* ui)
{
    snake_new_game(&ui->game, ui->level, ui->maze, ui->seed);
    ui->seed = (ui->game.rng + 1u) % 0xfff1u;
    ui->has_game = true;
    ui->new_record = false;
    enter(ui, UI_PLAY);
}

static void select_menu(SnakeUI* ui)
{
    unsigned selection = ui->selection;
    if (ui->has_game) {
        if (selection == 0) {
            enter(ui, UI_PLAY);
            return;
        }
        --selection;
    }
    switch (selection) {
    case 0:
        new_game(ui);
        break;
    case 1:
        enter(ui, UI_LEVEL);
        ui->selection = ui->level - 1;
        break;
    case 2:
        enter(ui, UI_MAZE);
        ui->selection = ui->maze;
        break;
    case 3:
        enter(ui, UI_TOP_SCORE);
        break;
    case 4:
        enter(ui, UI_INSTRUCTIONS);
        ui->instruction_line = 0;
        break;
    case 5:
        enter(ui, UI_ZOOM);
        ui->selection = ui->zoom - 1;
        break;
    }
}

void ui_action(SnakeUI* ui, UiAction action)
{
    ++ui->revision;
    if (ui->screen == UI_TITLE) {
        enter(ui, UI_MENU);
        return;
    }
    if (ui->screen == UI_PLAY) {
        switch (action) {
        case UI_UP:
            snake_apply_input(&ui->game, SNAKE_TURN_UP);
            break;
        case UI_DOWN:
            snake_apply_input(&ui->game, SNAKE_TURN_DOWN);
            break;
        case UI_LEFT:
            snake_apply_input(&ui->game, SNAKE_TURN_LEFT);
            break;
        case UI_RIGHT:
            snake_apply_input(&ui->game, SNAKE_TURN_RIGHT);
            break;
        case UI_BACK:
            enter(ui, UI_MENU);
            break;
        default:
            break;
        }
        return;
    }
    if (ui->screen == UI_DYING)
        return;
    if (ui->screen == UI_GAME_OVER) {
        enter(ui, UI_MENU);
        return;
    }
    if (ui->screen == UI_TOP_SCORE) {
        if (action == UI_SELECT || action == UI_BACK)
            enter(ui, UI_MENU);
        return;
    }
    if (ui->screen == UI_INSTRUCTIONS) {
        char lines[32][32];
        unsigned count = ui_instruction_lines(lines);
        if (action == UI_UP && ui->instruction_line)
            --ui->instruction_line;
        if (action == UI_DOWN && ui->instruction_line + 3 < count)
            ++ui->instruction_line;
        if (action == UI_BACK || action == UI_SELECT)
            enter(ui, UI_MENU);
        return;
    }
    if (ui->screen == UI_MENU) {
        unsigned count = ui_menu_count(ui);
        if (action == UI_UP)
            ui->selection = (ui->selection + count - 1) % count;
        if (action == UI_DOWN)
            ui->selection = (ui->selection + 1) % count;
        if (action == UI_SELECT)
            select_menu(ui);
        if (action == UI_BACK) {
            ui->title_step = 0;
            enter(ui, UI_TITLE);
        }
        return;
    }
    unsigned count = ui->screen == UI_LEVEL  ? 9
                     : ui->screen == UI_MAZE ? 6
                     : ui->screen == UI_ZOOM ? 3
                                             : 2;
    if (action == UI_UP || action == UI_LEFT)
        ui->selection = (ui->selection + count - 1) % count;
    if (action == UI_DOWN || action == UI_RIGHT)
        ui->selection = (ui->selection + 1) % count;
    if (action == UI_SELECT) {
        if (ui->screen == UI_LEVEL)
            ui->level = ui->selection + 1;
        if (ui->screen == UI_MAZE)
            ui->maze = ui->selection;
        if (ui->screen == UI_SOUNDS)
            ui->sounds = ui->selection == 0;
        if (ui->screen == UI_ZOOM)
            ui->zoom = ui->selection + 1;
        enter(ui, UI_MENU);
    }
    if (action == UI_BACK)
        enter(ui, UI_MENU);
}

void ui_keypad(SnakeUI* ui, unsigned key)
{
    ++ui->revision;
    if (ui->screen == UI_PLAY)
        snake_apply_keypad(&ui->game, key);
}

void ui_update(SnakeUI* ui, uint32_t elapsed_ms)
{
    /* Pause/restoration cannot advance an entire round after a long stall. */
    if (elapsed_ms > 250u)
        elapsed_ms = 250u;
    ui->elapsed_ms += elapsed_ms;
    if (ui->screen == UI_TITLE) {
        if (ui->elapsed_ms >= 2700ull)
            enter(ui, UI_MENU);
        else {
            unsigned step = ui->elapsed_ms / 250u;
            if (step != ui->title_step) {
                ui->title_step = step;
                ++ui->revision;
            }
        }
        return;
    }
    if (ui->screen == UI_DYING) {
        if (ui->elapsed_ms >= 2100ull) {
            ui->new_record = ui->game.score > ui->top_score;
            if (ui->game.score > ui->top_score) {
                ui->top_score = ui->game.score;
                ui->top_level = ui->game.level;
            }
            enter(ui, UI_GAME_OVER);
        } else {
            bool visible = (ui->elapsed_ms / 250u) % 2 != 0;
            if (visible != ui->snake_visible) {
                ui->snake_visible = visible;
                ++ui->revision;
            }
        }
        return;
    }
    if (ui->screen != UI_PLAY)
        return;
    ui->accumulator_ms += elapsed_ms;
    for (unsigned steps = 0; steps < 3; ++steps) {
        unsigned level_period_ms = ui->game.phase == SNAKE_CRASH_GRACE
                                       ? 100
                                       : snake_level_period_ms(ui->game.level);
        uint32_t period_ms = (uint32_t)level_period_ms * 1u;
        if (ui->accumulator_ms < period_ms)
            break;
        ui->accumulator_ms -= period_ms;
        snake_tick(&ui->game);
        ++ui->revision;
        if (ui->game.phase == SNAKE_DEAD || ui->game.phase == SNAKE_WON) {
            ui->has_game = false;
            enter(ui, UI_DYING);
            ui->snake_visible = false;
            break;
        }
    }
}

unsigned ui_instruction_lines(char lines[32][32])
{
    const char* p = firmware_snake_instructions;
    unsigned count = 0;
    while (*p && count < 32) {
        unsigned n = 0, last_space = 0;
        int width = 0;
        while (p[n] && n < 30) {
            int next_width =
                nokia_glyph(NOKIA_SMALL_PLAIN, (unsigned char)p[n])->width;
            if (width + next_width > 79)
                break;
            width += next_width;
            if (p[n] == ' ')
                last_space = n;
            ++n;
        }
        if (p[n] && last_space)
            n = last_space;
        memcpy(lines[count], p, n);
        lines[count++][n] = 0;
        p += n;
        while (*p == ' ')
            ++p;
    }
    return count;
}
