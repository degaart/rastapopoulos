#pragma once

#include "../../kernel/event_queue.h"
#include <stddef.h>
#include <stdint.h>

#define trace(...)                                                            \
    do {                                                                      \
        char buffer[128];                                                     \
        size_t buffer_len = snprintf(buffer, sizeof(buffer), __VA_ARGS__);    \
        sys_trace(__FILE__, strlen(__FILE__), __LINE__, buffer, buffer_len);  \
    } while (0)

int32_t syscall(uint32_t number, uint32_t param1, uint32_t param2,
                uint32_t param3, uint32_t param4, uint32_t param5);
int64_t syscall64(uint32_t number, uint32_t param1, uint32_t param2,
                  uint32_t param3, uint32_t param4, uint32_t param5);

uint64_t sys_getticks(void);
#define PERF_CLOCK_HZ 1193182u
/* Elapsed hardware clocks, including time spent with IRQ0 masked. */
uint64_t sys_perf_clock(void);
void sys_sleep(uint32_t ticks);
void sys_exit(uint8_t status) __attribute__((noreturn));
void sys_exit_thread(void) __attribute__((noreturn));
int sys_get_kbd_event(struct KeyEvent* evt, int* overflow, int block);
int sys_write(const char* s, size_t len);
int sys_exec(const char* command, const char* arguments);
int sys_shutdown(void);
int sys_list(void* buffer, size_t len);
int sys_lock_screen(void);
int sys_unlock_screen(void);
int sys_trace(const char* file, size_t file_len, int line, const char* s,
              size_t len);
int sys_vga_swap(void);
/* Inclusive damage bounds; all writes since the previous swap must be covered.
 * The kernel repairs stale damage on the inactive page before flipping it. */
int sys_vga_swap_rect(int x1, int y1, int x2, int y2);

int sys_vga_set_palette(uint8_t index, uint8_t red, uint8_t green,
                        uint8_t blue);

#define VGA_PHYSICAL_START 0xa0000
#define VGA_MODE_X_WIDTH   320
#define VGA_MODE_X_HEIGHT  240

/* Packed rows of 8-bit palette indices; pixels remain caller-owned. */
struct Bitmap
{
    int width;
    int height;
    uint8_t* pixels;
};

void vga_putpixel(unsigned x, unsigned y, uint8_t color);
/* Rectangle endpoints are inclusive, reversible and clipped to the screen. */
void vga_clear(uint8_t color);

/*
 * Draw a filled rectangle at the specified coordinates
 */
void vga_fillrect(int x1, int y1, int x2, int y2, uint8_t color);

/*
 * Draw a rectangle outline at the specified coordinates
 */
void vga_rect(int x1, int y1, int x2, int y2, uint8_t color);

/*
 * Draw a line
 */
void vga_drawline(int x1, int y1, int x2, int y2, uint8_t color);

/*
 * Draw an oval contained into the specified rectangle coordinates
 */
void vga_oval(int x1, int y1, int x2, int y2, uint8_t color);

/*
 * Draw a rounded rectangle
 */
void vga_roundrect(int x1, int y1, int x2, int y2, int radius, uint8_t color);

/*
 * Blit a bitmap into the specified coordinates. NULL selects the screen
 * backbuffer for either bitmap. Overlapping copies are supported.
 */
void vga_blit(struct Bitmap* src, int x1, int y1, struct Bitmap* dst, int x2,
              int y2, int width, int height);

