#include <stdio.h>
#include <stdarg.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>

#include "../debug.h"
#include "pmm.h"
#include "vmm.h"
#include "../../bootldr/kernel_params.h"
#include "idt.h"

unsigned char _TEXT_START_[0];

extern "C"
int main() {
    Bitset b0{32480};
    Bitset b1{159};
    
    LinkedList<Bitset> ll;
    ll.append(b0);
    exit(0);
    
    Bitset b{159};
    b.set_range(0, 158);
    assert(b.find() == 158);
    b.set(158);
    assert(b.find() == UINT_MAX);
    
    bios_memmap_t memmap[5];
    memset(memmap, 0, sizeof(memmap));
    memmap[0].base_lo = 0x00000000;
    memmap[0].size_lo = 0x0009FC00;
    memmap[0].flags = 0x00000001;
    
    memmap[1].base_lo = 0x0009FC00;
    memmap[1].size_lo = 0x00000400;
    memmap[1].flags = 0x00000002;
    
    memmap[2].base_lo = 0x000F0000;
    memmap[2].size_lo = 0x00010000;
    memmap[2].flags = 0x00000002;
    
    memmap[3].base_lo = 0x00100000;
    memmap[3].size_lo = 0x07EE0000;
    memmap[3].flags = 0x00000001;
    
    memmap[4].base_lo = 0x07FE0000;
    memmap[4].size_lo = 0x00020000;
    memmap[4].flags = 0x00000002;
    
//    memmap[5].base_lo = 0x08000000;
//    memmap[5].size_lo = 0xF0000000;
//    memmap[5].flags = 0x00000001;
    
    PMM::init(memmap, sizeof(memmap) / sizeof(*memmap));
    PMM::dump_zones();
    exit(0);
    
    /*
        Total memory: 0x7F7F000 bytes
        Number of pages: 0x7F7F
     */
    assert(PMM::pages_total() == 0x7F7F000 / 4096);
    assert(PMM::pages_free() == 0x7F7F000 / 4096);
    for(unsigned i = 0; i < 0x7EE0000; i += 4096) {
        PMM::alloc();
    }
    
    assert(PMM::pages_total() == 0x7F7F000 / 4096);
    assert(PMM::pages_free() == 0x9F000 / 4096);
    for(unsigned i = 0; i < 0x9F000; i += 4096) {
        PMM::alloc();
    }
    assert(PMM::pages_free() == 0);
    
    VMM::init();
    return 0;
}

void Debug::tracev(const char* file, unsigned line, const char* function, const char* format, va_list args) {
    char buffer[1024];
    strcpy(buffer, file);
    
    printf("[%s:%d %s] ", basename(buffer), line, function);
    vprintf(format, args);
    printf("\n");
}

void Debug::trace(const char* file, unsigned line, const char* function, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    tracev(file, line, function, fmt, args);
    va_end(args);
}

void Debug::panic(const char* file, unsigned line, const char* function, const char* fmt, ...) {
    char buffer[1024];
    strcpy(buffer, file);
    printf("[%s:%d %s] PANIC: ", basename(buffer), line, function);
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    abort();
}

uint32_t kheap_start() {
    void* phony = malloc(1);
    return (uint32_t)phony;
}

void* kmalloc_ap(uint32_t size, unsigned alignment, uint32_t* physical) {
    alignment = alignment ? alignment : 0;
    size = align(size, alignment);
//    int     posix_memalign(void **memptr, size_t alignment, size_t size);
    
    void* memptr;
    int ret = posix_memalign(&memptr, alignment, size);
    assert(ret == 0);
    return memptr;
}

void* kmalloc(uint32_t size) {
    return kmalloc_ap(size, 1, nullptr);
}

void kfree(void* ptr) {
//    free(ptr);
}

void IDT::install_handler(int num, isr_handler_t handler) {
    
}



