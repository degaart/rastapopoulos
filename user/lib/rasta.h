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

