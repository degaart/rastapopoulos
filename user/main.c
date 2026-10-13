#include "rasta.h"
#include <stdint.h>

enum
{
    WIDTH = 80,
    HEIGHT = 25,
    CELLS = WIDTH * HEIGHT,
    CAPACITY = (WIDTH - 2) * (HEIGHT - 2),
    STEP_TICKS = 10
};

enum
{
    RESULT_EXIT,
    RESULT_LOST,
    RESULT_WON
};

static volatile uint16_t* const vga = (volatile uint16_t*)(uintptr_t)0xb8000;

static uint16_t body[CAPACITY];
static uint8_t occupied[CELLS];
static unsigned tail, length;
static uint16_t food;
static uint32_t random_state;

static uint16_t saved[CELLS];
static uint16_t drawn[CELLS];
static uint8_t owned[CELLS];

static void put(unsigned pos, unsigned ch, unsigned color)
{
    uint16_t current = vga[pos];
    uint16_t value = (uint16_t)((color << 8) | ch);

    /* Save external contents, without saving our own graphics. */
    if (!owned[pos] || current != drawn[pos])
        saved[pos] = current;

    drawn[pos] = value;
    owned[pos] = 1;
    vga[pos] = value;
}

static void restore(unsigned pos)
{
    if (owned[pos]) {
        /*
         * Leave visible external changes alone.
         * This comparison cannot prevent concurrent write races.
         */
        if (vga[pos] == drawn[pos])
            vga[pos] = saved[pos];

        owned[pos] = 0;
    }
}

static void restore_all(void)
{
    unsigned pos;

    for (pos = 0; pos < CELLS; ++pos)
        restore(pos);
}

static void message(const char* text)
{
    unsigned pos;

    for (pos = 2; pos < WIDTH - 2; ++pos) {
        unsigned ch = ' ';

        if (*text)
            ch = (unsigned char)*text++;

        put(pos, ch, 0x0f);
    }
}

static uint32_t random_next(void)
{
    uint32_t x = random_state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    random_state = x;
    return x;
}

/* Called only when at least one playable cell is empty. */
static void place_food(void)
{
    unsigned choice = random_next() % (CAPACITY - length);
    unsigned x, y;

    for (y = 1; y < HEIGHT - 1; ++y) {
        for (x = 1; x < WIDTH - 1; ++x) {
            unsigned pos = y * WIDTH + x;

            if (!occupied[pos]) {
                if (choice == 0) {
                    food = (uint16_t)pos;
                    put(pos, '*', 0x0c);
                    return;
                }
                --choice;
            }
        }
    }
}

static int play_game(void)
{
    unsigned x, y, i;
    int dx = 1, dy = 0;
    int pending_dx = 1, pending_dy = 0;
    int turn_queued = 0;
    uint32_t last_step;

    restore_all();

    tail = 0;
    length = 3;
    random_state = sys_getticks() ^ UINT32_C(0x9e3779b9);
    if (random_state == 0)
        random_state = 1;

    /* Preserve the interior; draw only the border. */
    for (y = 0; y < HEIGHT; ++y) {
        for (x = 0; x < WIDTH; ++x) {
            unsigned pos = y * WIDTH + x;
            int wall = x == 0 || x == WIDTH - 1 || y == 0 || y == HEIGHT - 1;

            occupied[pos] = 0;
            if (wall)
                put(pos, '#', 0x07);
        }
    }

    message("SNAKE - Arrows move, Esc exits");

    for (i = 0; i < length; ++i) {
        unsigned pos = (HEIGHT / 2) * WIDTH + WIDTH / 2 - 2 + i;

        body[i] = (uint16_t)pos;
        occupied[pos] = 1;
        put(pos, i == length - 1 ? '@' : 'o', 0x0a);
    }

    place_food();
    last_step = sys_getticks();

    for (;;) {
        uint32_t now;
        unsigned head, next, old_tail;
        int nx, ny, growing;

        if (sys_key_down(KEY_ESC))
            return RESULT_EXIT;

        /* Queue at most one perpendicular turn per movement step. */
        if (!turn_queued) {
            if (dx != 0 && sys_key_down(KEY_UP)) {
                pending_dx = 0;
                pending_dy = -1;
                turn_queued = 1;
            } else if (dx != 0 && sys_key_down(KEY_DOWN)) {
                pending_dx = 0;
                pending_dy = 1;
                turn_queued = 1;
            } else if (dy != 0 && sys_key_down(KEY_LEFT)) {
                pending_dx = -1;
                pending_dy = 0;
                turn_queued = 1;
            } else if (dy != 0 && sys_key_down(KEY_RIGHT)) {
                pending_dx = 1;
                pending_dy = 0;
                turn_queued = 1;
            }
        }

        now = sys_getticks();
        if ((uint32_t)(now - last_step) < STEP_TICKS) {
            sys_sleep(1);
            continue;
        }
        last_step = now;

        dx = pending_dx;
        dy = pending_dy;
        turn_queued = 0;

        head = body[(tail + length - 1) % CAPACITY];
        nx = (int)(head % WIDTH) + dx;
        ny = (int)(head / WIDTH) + dy;

        if (nx <= 0 || nx >= WIDTH - 1 || ny <= 0 || ny >= HEIGHT - 1)
            return RESULT_LOST;

        next = (unsigned)ny * WIDTH + (unsigned)nx;
        old_tail = body[tail];
        growing = next == food;

        /* Entering the departing tail cell is legal. */
        if (occupied[next] && (growing || next != old_tail))
            return RESULT_LOST;

        put(head, 'o', 0x0a);

        if (!growing) {
            occupied[old_tail] = 0;
            restore(old_tail);
            tail = (tail + 1) % CAPACITY;
            --length;
        }

        body[(tail + length) % CAPACITY] = (uint16_t)next;
        ++length;
        occupied[next] = 1;
        put(next, '@', 0x0a);

        if (growing) {
            if (length == CAPACITY)
                return RESULT_WON;

            place_food();
        }
    }
}

/* Call from your OS's program entry point. */
void main(void)
{
    for (;;) {
        int result = play_game();
        int enter_released;

        if (result == RESULT_EXIT) {
            restore_all();
            return;
        }

        message(result == RESULT_WON
                    ? "YOU WIN! - Enter: restart | Esc: exit"
                    : "GAME OVER - Enter: restart | Esc: exit");

        /* Require a fresh Enter press after the game ends. */
        enter_released = !sys_key_down(KEY_ENTER);

        for (;;) {
            if (sys_key_down(KEY_ESC)) {
                restore_all();
                return;
            }

            if (!sys_key_down(KEY_ENTER)) {
                enter_released = 1;
            } else if (enter_released) {
                break;
            }

            sys_sleep(1);
        }
    }
}

