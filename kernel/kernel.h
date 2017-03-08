#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <stdarg.h>

void reboot();
void abort();
void halt();
extern void hlt();
extern void cli();
extern void sti();

extern unsigned char _KERNEL_START_;
extern unsigned char _KERNEL_END_;

extern uint32_t KERNEL_START;
extern uint32_t KERNEL_END;

extern unsigned char _TEXT_START_[];
extern unsigned char _TEXT_END_[];
extern unsigned char _RODATA_START_[];
extern unsigned char _RODATA_END_[];
extern unsigned char _DATA_START_[];
extern unsigned char _DATA_END_[];
extern unsigned char _USER_START_[];
extern unsigned char _USER_END_[];
extern unsigned char _USER_DATA_START_[];
extern unsigned char _USER_DATA_END_[];
extern unsigned char _BSS_START_[];
extern unsigned char _BSS_END_[];

extern unsigned char initial_kernel_stack[4096];

#define USERFUNC __attribute__((section(".user")))
#define USERDATA __attribute__((section(".userdata")))


