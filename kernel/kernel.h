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


