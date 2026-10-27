#include "maze.h"

#include <stddef.h>

typedef struct
{
    uint8_t x1, y1, x2, y2;
} WallSegment;

typedef struct
{
    const WallSegment* segments;
    uint8_t segment_count;
    uint8_t start_x, start_y;
} Maze;

/*
 * Decoded  at flash 0x3260E0..0x3261F7.
 * Each source segment uses 8 bytes: x1,y1,0,0,x2,y2,0,0. Segments are
 * inclusive. The first record's out-of-board segment is the No maze sentinel.
 * Record counts, pointers, and starts are at 0x326204..0x326263.
 */
static const WallSegment no_maze[] = {{255, 255, 255, 255}};
static const WallSegment maze_1[] = {
    {0, 0, 0, 8}, {0, 0, 19, 0}, {19, 0, 19, 8}, {0, 8, 19, 8}};
static const WallSegment maze_2[] = {
    {0, 0, 1, 0},  {0, 0, 0, 1}, {18, 0, 19, 0}, {19, 0, 19, 1},
    {0, 7, 0, 8},  {0, 8, 1, 8}, {18, 8, 19, 8}, {19, 7, 19, 8},
    {8, 3, 12, 3}, {8, 5, 12, 5}};
static const WallSegment maze_3[] = {
    {8, 0, 8, 4}, {12, 4, 12, 8}, {11, 2, 19, 2}, {0, 6, 9, 6}};
static const WallSegment maze_4[] = {
    {0, 0, 19, 0}, {0, 0, 0, 3},   {19, 0, 19, 3}, {0, 5, 0, 8},
    {0, 8, 19, 8}, {19, 5, 19, 8}, {7, 2, 7, 6},   {13, 2, 13, 6}};
static const WallSegment maze_5[] = {
    {0, 0, 0, 1},   {0, 0, 2, 0}, {5, 0, 16, 0}, {0, 3, 8, 3},
    {11, 3, 19, 3}, {8, 0, 8, 3}, {0, 6, 19, 6}, {11, 6, 11, 8}};

static const Maze mazes[SNAKE_MAZE_COUNT] = {
    {no_maze, 1, 4, 4}, {maze_1, 4, 4, 4}, {maze_2, 10, 0, 4},
    {maze_3, 4, 0, 5},  {maze_4, 8, 4, 7}, {maze_5, 8, 4, 4}};

/* Source bytes at 0x3261F8: 42 30 26 1e 17 12 0e 0b 09, times 10 ms. */
static const unsigned period_ms[SNAKE_LEVEL_COUNT] = {660, 480, 380, 300, 230,
                                                      180, 140, 110, 90};

bool snake_maze_start(unsigned maze, uint8_t* x, uint8_t* y)
{
    if (maze >= SNAKE_MAZE_COUNT || x == NULL || y == NULL)
        return false;
    *x = mazes[maze].start_x;
    *y = mazes[maze].start_y;
    return true;
}

bool snake_maze_wall(unsigned maze, uint8_t x, uint8_t y)
{
    if (maze >= SNAKE_MAZE_COUNT || x >= SNAKE_BOARD_WIDTH ||
        y >= SNAKE_BOARD_HEIGHT)
        return false;

    const Maze* record = &mazes[maze];
    for (unsigned i = 0; i < record->segment_count; ++i) {
        const WallSegment* segment = &record->segments[i];
        if (segment->x1 == segment->x2 && x == segment->x1 &&
            y >= segment->y1 && y <= segment->y2)
            return true;
        if (segment->y1 == segment->y2 && y == segment->y1 &&
            x >= segment->x1 && x <= segment->x2)
            return true;
    }
    return false;
}

unsigned snake_level_period_ms(unsigned level)
{
    if (level == 0 || level > SNAKE_LEVEL_COUNT)
        return 0;
    return period_ms[level - 1];
}
