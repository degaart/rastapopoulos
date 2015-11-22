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
    uint32_t size;
    uint32_t name_offset;
};

struct symtab_t {
    char* data;
    struct symbol_t* symtab;
    unsigned count;
};

static symtab_t _symtab;
extern uint8_t _initial_kernel_stack;

static const char* lookup_symbol(const struct symtab_t* symtab, uint32_t addr) {
    if(!symtab)
        return NULL;
    
    for(unsigned i = 0; i < symtab->count; i++) {
        if(addr >= symtab->symtab[i].addr && addr < symtab->symtab[i].addr + symtab->symtab[i].size) {
            return symtab->data + symtab->symtab[i].name_offset;
        }
    }
    return NULL;
}

void backtrace() {
    uint32_t* ebp;
    read_ebp(ebp);

    if(!_symtab.data) {
        TRACE("No backtrace available");
        return;
    }

    uint32_t stack_start, stack_end;
    Process* current_process = Process::current_process();
    if(current_process) {
        stack_start = (uint32_t)(current_process->kernel_stack());
        stack_end = (uint32_t) (current_process->kernel_stack() + 4096);
    } else {
        stack_start = (uint32_t) (&_initial_kernel_stack);
        stack_end = (uint32_t) (&_initial_kernel_stack + 4096);
    }

    uint32_t data[255];
    unsigned index = 0;
    while(1) {
        if((uint32_t)ebp <= stack_start || ((uint32_t)ebp+(sizeof(uint32_t)*2)) >= stack_end)
            break;

        uint32_t* prev_ebp = (uint32_t*) *ebp;
        if((uint32_t)prev_ebp <= stack_start || ((uint32_t)prev_ebp+(sizeof(uint32_t)*2)) >= stack_end)
            break;
        uint32_t prev_eip = *(prev_ebp + 1);

        data[index++] = prev_eip;
        ebp = prev_ebp;
    }
    
    TRACE("Backtrace:");
    for(unsigned i = 0; i < index; i++) {
        const char* name = lookup_symbol(&_symtab, data[i]);
        TRACE("\t0x%X %s", data[i], name ? name : "??");
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
            _symtab.data = (char*)kmalloc(size);
            memcpy(_symtab.data, start, size);
            _symtab.count = *((uint32_t*)_symtab.data);
            _symtab.symtab = (struct symbol_t*)(_symtab.data + sizeof(uint32_t));
            break;
        }
    }
    assert(_symtab.data);
}


