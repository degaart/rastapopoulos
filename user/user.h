#pragma once

#include <stdint.h>

uint32_t syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);
void debug_write(const char* msg);
int add(int a, int b, int c);
void halt(void);
uint32_t get_ticks(void);

