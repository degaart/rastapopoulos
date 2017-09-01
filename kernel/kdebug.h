#pragma once

#include "kernel.h"
#include "debug.h"

struct multiboot_info;

void backtrace();
void load_symbols(const struct multiboot_info* multiboot_info);
const char* lookup_function(uint32_t address);
void debug_printv(const char* fmt, va_list args);
void debug_printf(const char* fmt, ...);
void kdebug_init();

