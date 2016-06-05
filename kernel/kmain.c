#include "kernel.h"
#include "io.h"
#include "string.h"
#include "debug.h"
#include "registers.h"
#include "multiboot.h"

static void reboot()
{
    asm volatile(
        ".intel_syntax noprefix\n"
        "int 0x13\n"
    );
}


void fn3()
{
    backtrace();
}

void fn2()
{
    fn3();
}

void fn1()
{
    fn2();
}

void fn0()
{
    fn1();
}

void test_backtrace()
{
    fn0();
}

void kmain(const multiboot_info_t* multiboot_info)
{
    trace("Multiboot info: %p", multiboot_info);

    char multiboot_flags[32] = {};
    if(multiboot_info->flags & MULTIBOOT_FLAG_MEMINFO) {
        strlcat(multiboot_flags, "MEM ", sizeof(multiboot_flags));
        trace("Lower memory size: %dk", multiboot_info->mem_lower);
        trace("High memory size: %dk", multiboot_info->mem_upper);
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_MODINFO) {
        strlcat(multiboot_flags, "MOD ", sizeof(multiboot_flags));
        trace("Modules count: %d", multiboot_info->mods_count);
        trace("Modules load address: %p", multiboot_info->mods_addr);
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_SYMBOLS1) {
        strlcat(multiboot_flags, "SYM1 ", sizeof(multiboot_flags));
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_SYMBOLS2) {
        strlcat(multiboot_flags, "SYM2 ", sizeof(multiboot_flags));

        load_symbols(multiboot_info);
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_MMAP) {
        strlcat(multiboot_flags, "MMAP ", sizeof(multiboot_flags));
        trace("Memory map len: %d", multiboot_info->mmap_len);
        trace("Memory map load address: %p", multiboot_info->mmap_addr);
    }

    trace("Multiboot flags: %s", multiboot_flags);

    test_backtrace();
    reboot();

}

