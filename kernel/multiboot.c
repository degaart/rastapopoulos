#include "multiboot.h"
#include "heap.h"
#include "util.h"
#include "string.h"
#include "debug.h"
#include "kernel.h"
#include "elf.h"
#include "pmm.h"
#include "string.h"

static struct multiboot_info multiboot_info;
static struct heap* mi_heap;

void multiboot_init(const struct multiboot_info* init_mi)
{
    /* Prepare heap for multiboot data */
    unsigned char* heap_start = (unsigned char*)ALIGN(0x7C00, PAGE_SIZE);
    unsigned char* heap_end = (unsigned char*)0x9FBFF;
    unsigned heap_size = TRUNCATE(heap_end - heap_start, PAGE_SIZE);
    mi_heap = heap_init(heap_start,
                        heap_size,
                        heap_size);

    /* Init data */
    struct multiboot_info* mi = &multiboot_info;
    bzero(mi, sizeof(struct multiboot_info));

    if(init_mi->flags & MULTIBOOT_FLAG_CMDLINE) {
        mi->flags |= MULTIBOOT_FLAG_CMDLINE;

        unsigned cmdline_size = strlen(init_mi->cmdline);
        char* cmdline = heap_alloc(mi_heap,
                                   cmdline_size + 1);
        strlcpy(cmdline, init_mi->cmdline, cmdline_size + 1);
        mi->cmdline = cmdline;
    }

    if(init_mi->flags & MULTIBOOT_FLAG_MODINFO) {
        mi->flags |= MULTIBOOT_FLAG_MODINFO;

        mi->mods_count = init_mi->mods_count;
        mi->mods_addr = heap_alloc(mi_heap,
                                   init_mi->mods_count * sizeof(struct multiboot_mod_entry));

        for(int i = 0; i < mi->mods_count; i++) {
            struct multiboot_mod_entry* init_entry = init_mi->mods_addr + i;
            unsigned entry_size = init_entry->end - init_entry->start;
            struct multiboot_mod_entry* entry = mi->mods_addr + i;
            
            entry->start = heap_alloc(mi_heap, entry_size);
            memcpy(entry->start, init_entry->start, entry_size);
            entry->end = entry->start + entry_size;
            if(init_entry->str) {
                unsigned str_size = strlen(init_entry->str);
                entry->str = heap_alloc(mi_heap, str_size + 1);
                strlcpy(entry->str, init_entry->str, str_size + 1);
            } else {
                entry->str = NULL;
            }
        }
    }

    if(init_mi->flags & MULTIBOOT_FLAG_SYMBOLS2) {
        mi->flags |= MULTIBOOT_FLAG_SYMBOLS2;
        mi->sym2 = init_mi->sym2;

        elf32_shdr_t* init_shdrs = init_mi->sym2.addr;
        elf32_shdr_t* shdrs = heap_alloc(mi_heap, init_mi->sym2.size * mi->sym2.num);
        memcpy(shdrs, init_mi->sym2.addr, init_mi->sym2.size * mi->sym2.num);
        mi->sym2.addr = shdrs;

        elf32_shdr_t* shstr_hdr = init_shdrs + init_mi->sym2.shndx;
        const char* section_names = (const char*)shstr_hdr->sh_addr;

        /*
         * Sections to copy:
         *  init_mi->sym2.shndx
         *  type == SHT_SYMTAB
         *  name == ".strtab"
         */
        for(int i = 0; i < mi->sym2.num; i++) {
            bool copy = false;
            elf32_shdr_t* init_shdr = init_shdrs + i;
            if(init_shdr->sh_type == SHT_SYMTAB)
                copy = true;
            else if(i == init_mi->sym2.shndx)
                copy = true;
            else if(!strcmp(section_names + init_shdr->sh_name, ".strtab"))
                copy = true;

            if(copy) {
                elf32_shdr_t* shdr = shdrs + i;
                shdr->sh_addr = (uint32_t)heap_alloc(mi_heap, init_shdr->sh_size);
                memcpy((void*)shdr->sh_addr, 
                       (void*)init_shdr->sh_addr, 
                       init_shdr->sh_size);
            }
        }
    }

    if(init_mi->flags & MULTIBOOT_FLAG_MMAP) {
        mi->flags |= MULTIBOOT_FLAG_MMAP;
        mi->mmap_len = init_mi->mmap_len;
        mi->mmap_addr = heap_alloc(mi_heap, init_mi->mmap_len);

        memcpy(mi->mmap_addr, init_mi->mmap_addr, init_mi->mmap_len);
    }
}

void multiboot_dump()
{
    trace("Multiboot info: %p", &multiboot_info);

    char multiboot_flags[32] = {};
    if(multiboot_info.flags & MULTIBOOT_FLAG_MEMINFO) {
        strlcat(multiboot_flags, "MEM ", sizeof(multiboot_flags));
        trace("Lower memory size: %dk", multiboot_info.mem_lower);
        trace("High memory size: %dk", multiboot_info.mem_upper);
    }
    if(multiboot_info.flags & MULTIBOOT_FLAG_MODINFO) {
        strlcat(multiboot_flags, "MOD ", sizeof(multiboot_flags));
        trace("Modules count: %d", multiboot_info.mods_count);
        trace("Modules load address: %p", multiboot_info.mods_addr);
    }
    if(multiboot_info.flags & MULTIBOOT_FLAG_SYMBOLS1) {
        strlcat(multiboot_flags, "SYM1 ", sizeof(multiboot_flags));
    }
    if(multiboot_info.flags & MULTIBOOT_FLAG_SYMBOLS2) {
        strlcat(multiboot_flags, "SYM2 ", sizeof(multiboot_flags));
    }
    if(multiboot_info.flags & MULTIBOOT_FLAG_MMAP) {
        strlcat(multiboot_flags, "MMAP ", sizeof(multiboot_flags));
        trace("Memory map len: %d", multiboot_info.mmap_len);
        trace("Memory map load address: %p", multiboot_info.mmap_addr);
    }

    trace("Multiboot flags: %s", multiboot_flags);
    trace("_KERNEL_END_: %p", &_KERNEL_END_);
}

const struct multiboot_info* multiboot_get_info()
{
    return &multiboot_info;
}

struct heap_info multiboot_heap_info()
{
    return heap_info(mi_heap);
}



