#pragma once

#include "../../kernel/event_queue.h"
#include <stddef.h>
#include <stdint.h>

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

