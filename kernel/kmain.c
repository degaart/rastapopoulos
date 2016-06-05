#include "kernel.h"
#include "io.h"
#include "string.h"
#include "debug.h"

void kmain(uint32_t multiboot_info)
{
    trace("Hello, world!");
    asm volatile(
        ".intel_syntax noprefix\n"
        "int 0x13\n"
    );
}

