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
void sys_putpixel(unsigned x, unsigned y, uint8_t color);
int sys_trace(const char* file, size_t file_len, int line, const char* s,
              size_t len);
int sys_vga_swap(void);

/* Bitmap handles are process-owned; zero selects the screen backbuffer. */
typedef unsigned VGABitmap;
int sys_vga_fillrect(int x1, int y1, int x2, int y2, uint8_t color);
int sys_vga_rect(int x1, int y1, int x2, int y2, uint8_t color);
int sys_vga_drawline(int x1, int y1, int x2, int y2, uint8_t color);
int sys_vga_oval(int x1, int y1, int x2, int y2, uint8_t color);
int sys_vga_roundrect(int x1, int y1, int x2, int y2, int radius,
                      uint8_t color);
int sys_vga_blit(VGABitmap src, int x1, int y1, VGABitmap dst, int x2, int y2,
                 int width, int height);
VGABitmap sys_vga_load_bitmap(const void* buffer, size_t len, int width,
                              int height);
int sys_vga_destroy_bitmap(VGABitmap bitmap);
int sys_vga_set_palette(uint8_t index, uint8_t red, uint8_t green,
                        uint8_t blue);

