#ifndef SNAKE_CORE_H
#define SNAKE_CORE_H

#include "maze.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SNAKE_LEFT,
    SNAKE_UP,
    SNAKE_RIGHT,
    SNAKE_DOWN
} SnakeDirection;
typedef enum
{
    SNAKE_RUNNING,
    SNAKE_CRASH_GRACE,
    SNAKE_DEAD,
    SNAKE_WON
} SnakePhase;
typedef enum
{
    SNAKE_TURN_LEFT,
    SNAKE_TURN_UP,
    SNAKE_TURN_RIGHT,
    SNAKE_TURN_DOWN
} SnakeInput;

typedef struct
{
    uint8_t x, y;
} SnakeCell;

typedef struct
{
    unsigned level, maze;
    uint32_t rng;
    uint16_t length;
    SnakeCell body[SNAKE_BOARD_WIDTH * SNAKE_BOARD_HEIGHT]; /* tail to head */
    bool swallowed[SNAKE_BOARD_WIDTH * SNAKE_BOARD_HEIGHT];
    SnakeDirection direction, queued_direction, head_direction;
    SnakePhase phase;
    SnakeCell food;
    bool food_present;
    uint32_t score;
    unsigned grace_ticks;
    unsigned growth_pending;
    unsigned bonus_progress;
    bool special_eligible, special_collected;
    uint16_t collected_digits;
    uint32_t tick;
    bool mouth_open, growing_neck;
    SnakeCell bonus;
    bool bonus_present, bonus_large;
    unsigned bonus_kind, bonus_ticks, bonus_frame;
} SnakeGame;

typedef struct
{
    uint16_t length;
    SnakeCell body[SNAKE_BOARD_WIDTH * SNAKE_BOARD_HEIGHT];
    SnakeCell food;
    bool food_present;
    bool swallowed[SNAKE_BOARD_WIDTH * SNAKE_BOARD_HEIGHT];
    SnakeDirection direction;
    SnakePhase phase;
    uint32_t score;
    uint32_t tick;
    bool mouth_open, growing_neck;
    SnakeCell bonus;
    bool bonus_present, bonus_large;
    unsigned bonus_kind, bonus_ticks, bonus_frame;
} SnakeSnapshot;

bool snake_new_game(SnakeGame* game, unsigned level, unsigned maze,
                    uint32_t seed);
void snake_apply_input(SnakeGame* game, SnakeInput input);
void snake_apply_keypad(SnakeGame* game, unsigned key);
void snake_set_bonus_profile(SnakeGame* game, bool player_id_present,
                             uint16_t collected_digits);
void snake_tick(SnakeGame* game);
SnakeSnapshot snake_snapshot(const SnakeGame* game);
bool snake_occupied(const SnakeGame* game, unsigned x, unsigned y);

#endif
