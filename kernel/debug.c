#include "kernel.h"
#include "debug.h"
#include "string.h"
#include "io.h"
#include "elf.h"
#include "multiboot.h"
#include "registers.h"

extern uint8_t _initial_kernel_stack;
static unsigned _debug_sym_count = 0;
static elf32_sym_t* _debug_syms = NULL;
static const char* _debug_strtab = NULL;

static void __log_callback(int ch, void* unused)
{
    outb(0xE9, ch);
}

void __log(const char* func, const char* file, int line, const char* fmt, ...)
{
    format(__log_callback, NULL, "[%s:%d][%s] ", file, line, func);

    va_list args;
    va_start(args, fmt);
    formatv(__log_callback, NULL, fmt, args);
    va_end(args);

    __log_callback('\n', NULL);
}

void backtrace() {
    uint32_t* ebp;
    read_ebp(ebp);

    uint32_t stack_start = (uint32_t) (&_initial_kernel_stack);
    uint32_t stack_end = (uint32_t) (&_initial_kernel_stack + 4096);

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
    
    trace("Backtrace:");
    for(unsigned i = 0; i < index; i++) {
        elf32_sym_t* sym = _debug_syms;
        const char* name = NULL;
        for(unsigned sym_index = 0; sym_index < _debug_sym_count; sym_index++) {
            if(ELF32_ST_TYPE(sym->st_info) == STT_FUNC || 
               ELF32_ST_TYPE(sym->st_info) == STT_OBJECT) {
                
                if(data[i] >= sym->st_value && data[i] < sym->st_value + sym->st_size) {
                    name = _debug_strtab + sym->st_name;
                }
            }
            sym++;
        }
        trace("\t0x%X %s", data[i], name ? name : "??");
    }
}

void load_symbols(const void* multiboot_info_ptr)
{
    const multiboot_info_t* multiboot_info = multiboot_info_ptr;

    // All headers
    elf32_shdr_t* hdrs = (elf32_shdr_t*)multiboot_info->sym2.addr;
   
    // Section name table header
    elf32_shdr_t* shstr_hdr = &hdrs[multiboot_info->sym2.shndx];
    const char* section_names = (const char*)shstr_hdr->sh_addr;

    // Symbols table header
    elf32_shdr_t* sym_hdr = NULL;

    // Symbol names table header
    elf32_shdr_t* strtab_hdr = NULL;
    
    // Find relevant sections
    for(unsigned i = 0; i < multiboot_info->sym2.num; i++) {
        if(hdrs[i].sh_type == SHT_SYMTAB) {
            sym_hdr = &hdrs[i];
        } else if(!strcmp(section_names + hdrs[i].sh_name, ".strtab")) {
            strtab_hdr = &hdrs[i];
        }

        if(sym_hdr && strtab_hdr)
            break;
    }

    // Dump all this
    if(sym_hdr && strtab_hdr) {
        _debug_strtab = (const char*)strtab_hdr->sh_addr;
        _debug_sym_count = sym_hdr->sh_size / sizeof(elf32_sym_t);
        _debug_syms = (elf32_sym_t*)sym_hdr->sh_addr;
    }
}


