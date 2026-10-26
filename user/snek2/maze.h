#ifndef SNAKE_MAZE_H
#define SNAKE_MAZE_H

#include <stdbool.h>
#include <stdint.h>

enum
{
    SNAKE_BOARD_WIDTH = 20,
    SNAKE_BOARD_HEIGHT = 9,
    SNAKE_MAZE_COUNT = 6,
    SNAKE_LEVEL_COUNT = 9
};

/* Maze indices are 0 (No maze) through 5. Invalid indices return false. */
bool snake_maze_start(unsigned maze, uint8_t* x, uint8_t* y);
bool snake_maze_wall(unsigned maze, uint8_t x, uint8_t y);

/* Levels are 1 through 9. Invalid levels return 0. */
unsigned snake_level_period_ms(unsigned level);

#endif
