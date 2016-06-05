#pragma once

void __log(const char* func, const char* file, int line, const char* fmt, ...);

#define trace(...) \
    __log(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)

struct multiboot_info;

void backtrace();
void load_symbols(const struct multiboot_info* multiboot_info);
