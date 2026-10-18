#include "../../kernel/scancode.h"
#include <rasta.h>

enum
{
    WIDTH = 80,
    HEIGHT = 25,
    TOP = 2,
    BOTTOM = HEIGHT - 1,
    CELLS = WIDTH * HEIGHT,
    CAPACITY = (WIDTH - 2) * (BOTTOM - TOP - 1),
    START_STEP_TICKS = 20,
    MIN_STEP_TICKS = 2,
    SCORE_X = 63
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
static unsigned tail, length, score;
static uint16_t food;
static uint32_t random_state;

static void put(unsigned pos, unsigned ch, unsigned color)
{
    vga[pos] = (uint16_t)((color << 8) | ch);
}

static void draw_screen(void)
{
    unsigned x, y;

    for (y = TOP; y < HEIGHT; ++y)
        for (x = 0; x < WIDTH; ++x)
            put(y * WIDTH + x, ' ', 0x07);

    for (x = 1; x < WIDTH - 1; ++x) {
        put(TOP * WIDTH + x, 0xc4, 0x07);
        put(BOTTOM * WIDTH + x, 0xc4, 0x07);
    }

    for (y = TOP + 1; y < BOTTOM; ++y) {
        put(y * WIDTH, 0xb3, 0x07);
        put(y * WIDTH + WIDTH - 1, 0xb3, 0x07);
    }

    put(TOP * WIDTH, 0xda, 0x07);
    put(TOP * WIDTH + WIDTH - 1, 0xbf, 0x07);
    put(BOTTOM * WIDTH, 0xc0, 0x07);
    put(BOTTOM * WIDTH + WIDTH - 1, 0xd9, 0x07);
}

static void draw_score(void)
{
    static const char label[] = " Score: ";
    unsigned pos = TOP * WIDTH + SCORE_X;
    unsigned value = score;
    unsigned divisor = 1000;
    unsigned i;
    int started = 0;

    for (i = 0; i < sizeof(label) - 1; ++i)
        put(pos++, (unsigned char)label[i], 0x0e);

    while (divisor != 0) {
        unsigned digit = value / divisor;

        if (digit != 0 || divisor == 1)
            started = 1;

        put(pos++, started ? '0' + digit : ' ', 0x0e);
        value %= divisor;
        divisor /= 10;
    }

    put(pos, ' ', 0x0e);
}

static void message(const char* text)
{
    unsigned x;

    for (x = 2; x < SCORE_X; ++x) {
        if (*text)
            put(TOP * WIDTH + x, (unsigned char)*text++, 0x0f);
        else
            put(TOP * WIDTH + x, 0xc4, 0x07);
    }

    draw_score();
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

static void place_food(void)
{
    unsigned choice = random_next() % (CAPACITY - length);
    unsigned x, y;

    for (y = TOP + 1; y < BOTTOM; ++y) {
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
    unsigned i;
    unsigned step_ticks = START_STEP_TICKS;
    int dx = 1, dy = 0;
    int pending_dx = 1, pending_dy = 0;
    int turn_queued = 0;
    uint64_t last_step;

    tail = 0;
    length = 3;
    score = 0;
    random_state = (uint32_t)sys_getticks() ^ UINT32_C(0x9e3779b9);
    if (random_state == 0)
        random_state = 1;

    for (i = 0; i < CELLS; ++i)
        occupied[i] = 0;

    draw_screen();
    message(" SNAKE - Arrows move, Esc exits ");

    for (i = 0; i < length; ++i) {
        unsigned pos = ((TOP + BOTTOM) / 2) * WIDTH + WIDTH / 2 - 2 + i;

        body[i] = (uint16_t)pos;
        occupied[pos] = 1;
        put(pos, i == length - 1 ? '@' : 'o', 0x0a);
    }

    place_food();
    last_step = sys_getticks();

    for (;;) {
        uint64_t now;
        unsigned head, next, old_tail;
        int nx, ny, growing;

        if (sys_key_down(SCANCODE_ESC))
            return RESULT_EXIT;

        if (!turn_queued) {
            if (sys_key_down(SCANCODE_F1)) {
                volatile uint32_t* ptr = (volatile uint32_t*)0x3000;
                *ptr = 0xDEADBEEF;
            }
            if (sys_key_down(SCANCODE_F2)) {
                asm volatile("mov ax, 0x30\n\t"
                             "mov es, ax"
                             :
                             :
                             : "ax", "memory");
            }
            if (sys_key_down(SCANCODE_F3)) {
                volatile int num = 5, div = 0;
                volatile int quotient = num / div;
            }
            if (sys_key_down(SCANCODE_F4)) {
                asm volatile("int 3");
            }
            if (dx != 0 && sys_key_down(SCANCODE_UP)) {
                pending_dx = 0;
                pending_dy = -1;
                turn_queued = 1;
            } else if (dx != 0 && sys_key_down(SCANCODE_DOWN)) {
                pending_dx = 0;
                pending_dy = 1;
                turn_queued = 1;
            } else if (dy != 0 && sys_key_down(SCANCODE_LEFT)) {
                pending_dx = -1;
                pending_dy = 0;
                turn_queued = 1;
            } else if (dy != 0 && sys_key_down(SCANCODE_RIGHT)) {
                pending_dx = 1;
                pending_dy = 0;
                turn_queued = 1;
            }
        }

        now = sys_getticks();
        if ((uint64_t)(now - last_step) < step_ticks) {
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

        if (nx <= 0 || nx >= WIDTH - 1 || ny <= TOP || ny >= BOTTOM)
            return RESULT_LOST;

        next = (unsigned)ny * WIDTH + (unsigned)nx;
        old_tail = body[tail];
        growing = next == food;

        if (occupied[next] && (growing || next != old_tail))
            return RESULT_LOST;

        put(head, 'o', 0x0a);

        if (!growing) {
            occupied[old_tail] = 0;
            put(old_tail, ' ', 0x07);
            tail = (tail + 1) % CAPACITY;
            --length;
        }

        body[(tail + length) % CAPACITY] = (uint16_t)next;
        ++length;
        occupied[next] = 1;
        put(next, '@', 0x0a);

        if (growing) {
            ++score;
            draw_score();

            if (length == CAPACITY)
                return RESULT_WON;

            if (step_ticks > MIN_STEP_TICKS)
                --step_ticks;

            place_food();
        }
    }
}

int main(void)
{
    for (;;) {
        int result = play_game();
        int enter_released;

        if (result == RESULT_EXIT)
            sys_exit(0);

        message(result == RESULT_WON
                    ? " YOU WIN! - Enter: restart | Esc: exit "
                    : " GAME OVER - Enter: restart | Esc: exit ");

        enter_released = !sys_key_down(SCANCODE_ENTER);

        for (;;) {
            if (sys_key_down(SCANCODE_ESC))
                sys_exit(0);

            if (!sys_key_down(SCANCODE_ENTER)) {
                enter_released = 1;
            } else if (enter_released) {
                break;
            }

            sys_sleep(1);
        }
    }
}

