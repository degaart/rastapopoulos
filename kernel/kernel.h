#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <stdarg.h>

void reboot();
void halt();
extern void hlt();

extern unsigned char _KERNEL_START_[];
extern unsigned char _KERNEL_END_[];
extern unsigned char _TEXT_START_[];
extern unsigned char _TEXT_END_[];
extern unsigned char _RODATA_START_[];
extern unsigned char _RODATA_END_[];
extern unsigned char _DATA_START_[];
extern unsigned char _DATA_END_[];
extern unsigned char _BSS_START_[];
extern unsigned char _BSS_END_[];

#define KERNEL_BASE_ADDR    0xC0000000

extern unsigned char initial_kernel_stack[4096];


