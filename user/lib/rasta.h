#pragma once

#include <stdint.h>

#define KEY_LEFT  0xE04B
#define KEY_RIGHT 0xE04D
#define KEY_UP    0xE048
#define KEY_DOWN  0xE050
#define KEY_ESC   0x01
#define KEY_ENTER 0x1C

int32_t syscall(uint32_t number, uint32_t param1, uint32_t param2,
                uint32_t param3, uint32_t param4, uint32_t param5);
int64_t syscall64(uint32_t number, uint32_t param1, uint32_t param2,
                  uint32_t param3, uint32_t param4, uint32_t param5);

uint64_t sys_getticks(void);
void sys_sleep(uint32_t ticks);
int sys_key_down(unsigned scancode);
void sys_exit(uint8_t status) __attribute__((noreturn));
void sys_exit_thread(void) __attribute__((noreturn));

