#include "kernel.h"
#include "io.h"
#include "string.h"
#include "debug.h"
#include "registers.h"

static void reboot()
{
    asm volatile(
        ".intel_syntax noprefix\n"
        "int 0x13\n"
    );
}

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    union {
        struct {
            uint32_t tabsize;
            uint32_t strsize;
            uint32_t addr;
            uint32_t reserved;
        } sym1;

        struct {
            uint32_t num;
            uint32_t size;
            uint32_t addr;
            uint32_t shndx;
        } sym2;
    };
    uint32_t mmap_len;
    uint32_t mmap_addr;
    uint32_t drives_len;
    uint32_t drivers_addr;
    uint32_t config_table;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint32_t vbe_mode;
    uint32_t vbe_interface_seg;
    uint32_t vbe_interface_off;
    uint32_t vbe_interface_len;
} multiboot_info_t;

#define MULTIBOOT_FLAG_MEMINFO  (1 << 0)
#define MULTIBOOT_FLAG_MODINFO  (1 << 3)
#define MULTIBOOT_FLAG_SYMBOLS1 (1 << 4)
#define MULTIBOOT_FLAG_SYMBOLS2 (1 << 5)
#define MULTIBOOT_FLAG_MMAP     (1 << 6)

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} elf32_shdr_t;

#define SHT_NULL                0   
#define SHT_SYMTAB              2
#define SHT_STRTAB              3

typedef struct {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
} elf32_sym_t;

#define STN_UNDEF               0

#define STT_NOTYPE              0
#define STT_OBJECT              1
#define STT_FUNC                2
#define STT_SECTION             3
#define STT_FILE                4
#define STT_LOPROC              13
#define STT_HIPROC              15

#define ELF32_ST_BIND(i)        ((i) >> 4)
#define ELF32_ST_TYPE(i)        ((i) & 0xF)
#define ELF32_ST_INFO(b, t)     (((b) << 4) | ((t) & 0xF))

extern uint8_t _initial_kernel_stack;
static unsigned _debug_sym_count = 0;
static elf32_sym_t* _debug_syms = NULL;
static const char* _debug_strtab = NULL;

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

        trace("shndx: %p", multiboot_info->sym2.shndx);

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
            trace("section %d: %s", i, section_names + hdrs[i].sh_name);

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
#if 0
            const char* strtab = (const char*)strtab_hdr->sh_addr;

            unsigned sym_count = sym_hdr->sh_size / sizeof(elf32_sym_t);
            elf32_sym_t* sym = (elf32_sym_t*)sym_hdr->sh_addr;
            for(unsigned i = 0; i < sym_count; i++) {
                if(ELF32_ST_TYPE(sym->st_info) == STT_FUNC || ELF32_ST_TYPE(sym->st_info) == STT_OBJECT) {
                    
                    trace("sym[%d]: %p %p %s", i, sym->st_value, sym->st_size, strtab + sym->st_name);
                }
                sym++;
            }
            trace("Sym count: %d", sym_count);
#endif
        }
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

