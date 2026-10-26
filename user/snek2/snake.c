#include "snake.h"

#include <string.h>

/* v05.79 0x2dc980: the game's private generator, not libc rand(). */
static uint32_t random_next(SnakeGame* game)
{
    game->rng = (0x3623u + game->rng * 0x625fu) % 0xfff1u;
    return game->rng;
}

static bool same_cell(SnakeCell a, SnakeCell b)
{
    return a.x == b.x && a.y == b.y;
}

static SnakeCell advance(SnakeCell head, SnakeDirection direction)
{
    switch (direction) {
    case SNAKE_LEFT:
        head.x =
            (uint8_t)((head.x + SNAKE_BOARD_WIDTH - 1) % SNAKE_BOARD_WIDTH);
        break;
    case SNAKE_UP:
        head.y =
            (uint8_t)((head.y + SNAKE_BOARD_HEIGHT - 1) % SNAKE_BOARD_HEIGHT);
        break;
    case SNAKE_RIGHT:
        head.x = (uint8_t)((head.x + 1) % SNAKE_BOARD_WIDTH);
        break;
    case SNAKE_DOWN:
        head.y = (uint8_t)((head.y + 1) % SNAKE_BOARD_HEIGHT);
        break;
    }
    return head;
}

bool snake_occupied(const SnakeGame* game, unsigned x, unsigned y)
{
    if (x >= SNAKE_BOARD_WIDTH || y >= SNAKE_BOARD_HEIGHT)
        return true;
    for (unsigned i = 0; i < game->length; ++i)
        if (game->body[i].x == x && game->body[i].y == y)
            return true;
    return false;
}

static bool blocked(const SnakeGame* game, unsigned x, unsigned y)
{
    return snake_maze_wall(game->maze, (uint8_t)x, (uint8_t)y) ||
           snake_occupied(game, x, y);
}

static bool bonus_contains(const SnakeGame* game, SnakeCell cell)
{
    return game->bonus_present && cell.x >= game->bonus.x &&
           cell.x <= game->bonus.x + 1 && cell.y >= game->bonus.y &&
           cell.y <= game->bonus.y + (game->bonus_large ? 1 : 0);
}

/* 0x2739b2 excludes every bonus column AND row on the random path. The
   fallback 0x27391a scans ordinary occupancy only, including bonus rows. */
static bool food_candidate(const SnakeGame* game, unsigned x, unsigned y)
{
    if (blocked(game, x, y))
        return false;
    if (!game->bonus_present)
        return true;
    return x != game->bonus.x && x != game->bonus.x + 1u &&
           y != game->bonus.y &&
           (!game->bonus_large || y != game->bonus.y + 1u);
}

static void spawn_food(SnakeGame* game)
{
    for (unsigned tries = 0; tries < 100; ++tries) {
        unsigned x = random_next(game) % SNAKE_BOARD_WIDTH;
        unsigned y = random_next(game) % SNAKE_BOARD_HEIGHT;
        if (food_candidate(game, x, y)) {
            game->food = (SnakeCell) {(uint8_t)x, (uint8_t)y};
            game->food_present = true;
            return;
        }
    }
    for (unsigned y = 0; y < SNAKE_BOARD_HEIGHT; ++y)
        for (unsigned x = 0; x < SNAKE_BOARD_WIDTH; ++x)
            if (!blocked(game, x, y)) {
                game->food = (SnakeCell) {(uint8_t)x, (uint8_t)y};
                game->food_present = true;
                return;
            }
    game->food_present = false;
}

/* 0x273aec: the 2x2 creature additionally needs a nonempty phone player
   profile (0x11114f), solo mode, and no previous special collection this game.
 */
static void spawn_bonus(SnakeGame* game)
{
    bool large = random_next(game) % 50 == 0 && game->special_eligible &&
                 !game->special_collected;
    game->bonus_present = false;
    game->bonus_large = large;
    game->bonus_frame = 0;
    for (unsigned tries = 0; tries < 100; ++tries) {
        unsigned x = random_next(game) % (SNAKE_BOARD_WIDTH - 1);
        unsigned y =
            random_next(game) % (SNAKE_BOARD_HEIGHT - (large ? 1 : 0));
        bool free = !blocked(game, x, y) && !blocked(game, x + 1, y);
        if (large)
            free = free && !blocked(game, x, y + 1) &&
                   !blocked(game, x + 1, y + 1);
        if (free &&
            (!game->food_present ||
             (x != game->food.x && x + 1 != game->food.x &&
              y != game->food.y && (!large || y + 1 != game->food.y)))) {
            game->bonus = (SnakeCell) {(uint8_t)x, (uint8_t)y};
            game->bonus_present = true;
            break;
        }
    }
    if (!large)
        game->bonus_kind = random_next(game) % 6;
    game->bonus_ticks = game->bonus_present ? game->bonus_progress : 0;
    if (!game->bonus_present)
        game->bonus_progress = 0;
}

void snake_set_bonus_profile(SnakeGame* game, bool player_id_present,
                             uint16_t collected_digits)
{
    if (!game)
        return;
    game->special_eligible = player_id_present;
    game->collected_digits = collected_digits & 0x3ffu;
}

bool snake_new_game(SnakeGame* game, unsigned level, unsigned maze,
                    uint32_t seed)
{
    if (!game || snake_level_period_ms(level) == 0 || maze >= SNAKE_MAZE_COUNT)
        return false;
    memset(game, 0, sizeof(*game));
    game->level = level;
    game->maze = maze;
    game->rng = seed;
    game->direction = game->queued_direction = game->head_direction =
        SNAKE_RIGHT;
    game->phase = SNAKE_RUNNING;
    uint8_t start_x, start_y;
    if (!snake_maze_start(maze, &start_x, &start_y))
        return false;
    /* 0x275924 advances seven times then vacates the original cell. */
    for (unsigned i = 1; i <= 7; ++i) {
        uint8_t x = (uint8_t)((start_x + i) % SNAKE_BOARD_WIDTH);
        if (snake_maze_wall(maze, x, start_y))
            return false;
        game->body[game->length++] = (SnakeCell) {x, start_y};
    }
    spawn_food(game);
    return true;
}

void snake_apply_input(SnakeGame* game, SnakeInput input)
{
    if (!game || game->phase == SNAKE_DEAD || game->phase == SNAKE_WON)
        return;
    if ((unsigned)input > SNAKE_TURN_DOWN)
        return;
    SnakeDirection next = (SnakeDirection)input;
    if (((unsigned)next + 2u) % 4u != (unsigned)game->direction)
        game->queued_direction = next;
}

void snake_apply_keypad(SnakeGame* game, unsigned key)
{
    if (!game)
        return;
    bool vertical =
        game->direction == SNAKE_UP || game->direction == SNAKE_DOWN;
    SnakeInput input;
    switch (key) {
    case 1:
        input = vertical ? SNAKE_TURN_LEFT : SNAKE_TURN_UP;
        break;
    case 2:
        input = SNAKE_TURN_UP;
        break;
    case 3:
        input = vertical ? SNAKE_TURN_RIGHT : SNAKE_TURN_UP;
        break;
    case 4:
        input = SNAKE_TURN_LEFT;
        break;
    case 6:
        input = SNAKE_TURN_RIGHT;
        break;
    case 7:
        input = vertical ? SNAKE_TURN_LEFT : SNAKE_TURN_DOWN;
        break;
    case 8:
        input = SNAKE_TURN_DOWN;
        break;
    case 9:
        input = vertical ? SNAKE_TURN_RIGHT : SNAKE_TURN_DOWN;
        break;
    default:
        return;
    }
    snake_apply_input(game, input);
}

void snake_tick(SnakeGame* game)
{
    if (!game || game->phase == SNAKE_DEAD || game->phase == SNAKE_WON)
        return;
    /* Handler animates a special creature before the collision check. */
    if (game->bonus_present && game->bonus_large)
        game->bonus_frame ^= 1u;
    game->direction = game->queued_direction;
    SnakeCell head = advance(game->body[game->length - 1], game->direction);
    bool tail_vacates = game->growth_pending == 0;
    bool hits_body = false;
    for (unsigned i = tail_vacates ? 1 : 0; i < game->length; ++i)
        if (same_cell(game->body[i], head)) {
            hits_body = true;
            break;
        }
    if (snake_maze_wall(game->maze, head.x, head.y) || hits_body) {
        /* Handler 0x2750e8 rechecks the move after 100ms: a new turn can
         * rescue it. */
        game->phase =
            game->phase == SNAKE_CRASH_GRACE ? SNAKE_DEAD : SNAKE_CRASH_GRACE;
        return;
    }
    game->phase = SNAKE_RUNNING;
    game->head_direction = game->direction;
    game->growing_neck = !tail_vacates;
    if (!tail_vacates)
        game->swallowed[game->length - 1] = true;
    if (tail_vacates) {
        memmove(game->body, game->body + 1,
                (game->length - 1) * sizeof(game->body[0]));
        memmove(game->swallowed, game->swallowed + 1,
                (game->length - 1) * sizeof(game->swallowed[0]));
        --game->length;
    }
    game->growth_pending = 0;
    if (game->length < SNAKE_BOARD_WIDTH * SNAKE_BOARD_HEIGHT) {
        game->swallowed[game->length] = false;
        game->body[game->length++] = head;
    }
    ++game->tick;
    /* 0x27409c runs before replacement food is placed. */
    SnakeCell ahead = advance(head, game->direction);
    game->mouth_open = (game->food_present && same_cell(ahead, game->food)) ||
                       bonus_contains(game, ahead);
    bool eating_bonus = bonus_contains(game, head);
    /* advance_head clears the creature half ahead before its mouth check. */
    if (eating_bonus)
        game->mouth_open = game->food_present && same_cell(ahead, game->food);
    if (game->bonus_present && !eating_bonus && game->bonus_ticks) {
        if (--game->bonus_ticks == 0) {
            game->bonus_present = false;
            game->bonus_progress = 0;
        }
    }
    bool eating_food = game->food_present && same_cell(head, game->food);
    if (eating_bonus) {
        if (game->bonus_large) {
            /* 0x274afa records one new digit. Avoid the firmware's unbounded
               retry if a host supplies an already-complete profile bitset. */
            if (game->collected_digits != 0x3ffu) {
                unsigned bit;
                do {
                    bit = 1u << (random_next(game) % 10);
                } while (game->collected_digits & bit);
                game->collected_digits |= (uint16_t)bit;
            }
            game->special_collected = true;
        }
        game->score += game->bonus_ticks * 2 + game->level * 5 + 5;
        game->bonus_present = false;
        game->bonus_ticks = game->bonus_progress = 0;
        game->growth_pending = 1;
    } else if (eating_food) {
        game->score += game->level;
        spawn_food(game);
        if (!game->bonus_present)
            game->bonus_progress += 4;
        if (game->bonus_progress >= 20 && !game->bonus_present)
            spawn_bonus(game);
        game->growth_pending = 1;
    }
    /* A full board hides food but does not itself end the firmware game.
       Tail-following moves can still be legal. The phone's299-slot ring
       cannot be exhausted by180 unique physical cells. */
}

SnakeSnapshot snake_snapshot(const SnakeGame* game)
{
    SnakeSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    if (!game)
        return snapshot;
    snapshot.length = game->length;
    memcpy(snapshot.body, game->body, game->length * sizeof(game->body[0]));
    memcpy(snapshot.swallowed, game->swallowed,
           game->length * sizeof(game->swallowed[0]));
    snapshot.food = game->food;
    snapshot.food_present = game->food_present;
    snapshot.direction = game->head_direction;
    snapshot.phase = game->phase;
    snapshot.score = game->score;
    snapshot.tick = game->tick;
    snapshot.mouth_open = game->mouth_open;
    snapshot.growing_neck = game->growing_neck;
    snapshot.bonus = game->bonus;
    snapshot.bonus_present = game->bonus_present;
    snapshot.bonus_large = game->bonus_large;
    snapshot.bonus_kind = game->bonus_kind;
    snapshot.bonus_ticks = game->bonus_ticks;
    snapshot.bonus_frame = game->bonus_frame;
    return snapshot;
}
