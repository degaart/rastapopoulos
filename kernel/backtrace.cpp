#include "backtrace.h"
#include <stdint.h>
#include "debug.h"
#include "util.h"
#include "regs.h"
#include "multiboot.h"
#include "kmalloc.h"
#include "string.h"
#include "process.h"

struct symbol_t {
    uint32_t addr;
    uint32_t name_offset;
};
static symbol_t* _symbols = nullptr;
extern uint8_t _initial_kernel_stack;

static const char* lookup_symbol(uint32_t addr) {
    if(!_symbols)
        return nullptr;
    
    for(int i = 0; _symbols[i].addr && _symbols[i].name_offset; i++) {
        if(addr >= _symbols[i].addr && addr <= _symbols[i+1].addr) {
            return ((const char*)_symbols) + _symbols[i].name_offset;
        }
    }
    return nullptr;
}

void backtrace() {
    uint32_t* ebp;
    read_ebp(ebp);

    //TRACE("Initial kernel stack: %p", &_initial_kernel_stack);
    TRACE("Backtrace:");
    while(1) {
        uint32_t* prev_ebp = (uint32_t*) *ebp;
        uint32_t prev_eip = *(prev_ebp + 1);

        const char* name = lookup_symbol(prev_eip);
        TRACE("\t0x%X %s", prev_eip, name ? name : "??");

        ebp = prev_ebp;
        if((uint8_t*)ebp <= &_initial_kernel_stack || (uint8_t*)ebp >= &_initial_kernel_stack + 4096) {
            Process* current_process = Process::current_process();
            if(!current_process)
                break;
            if((uint8_t*)ebp <= current_process->kernel_stack() || (uint8_t*)ebp >= current_process->kernel_stack() + 4096)
                break;
        }
    }
}

void load_symbols(multiboot_info_t* multiboot_info) {
    assert(multiboot_info->flags & MULTIBOOT_INFO_MODS);

    void* syms = nullptr;
    unsigned syms_size = 0;
    multiboot_mod_list* mods = (multiboot_mod_list*)multiboot_info->mods_addr;
    for(unsigned i = 0; i < multiboot_info->mods_count; i++) {
        const char* cmdline = (char*)mods[i].cmdline;
        const void* start = (void*)mods[i].mod_start;
        unsigned size = mods[i].mod_end - mods[i].mod_start;

        if(!strcmp(cmdline, "symbols")) {
            syms = (void*)mods[i].mod_start;
            syms_size = size;
            break;
        }
    }
    assert(syms);

    _symbols = (symbol_t*)kmalloc(syms_size);
    memcpy(_symbols, syms, syms_size);
}


