#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <stdarg.h>

void reboot();
void halt();
extern void hlt();
extern void cli();
extern void sti();

extern unsigned char _KERNEL_START_;
extern unsigned char _KERNEL_END_;

uint32_t KERNEL_START;
uint32_t KERNEL_END;


