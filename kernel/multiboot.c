#include "multiboot.h"
#include "heap.h"
#include "util.h"
#include "string.h"
#include "debug.h"
#include "kernel.h"
#include "elf.h"
#include "pmm.h"
#include "string.h"

static const struct multiboot_info* multiboot_info;

/*
 * Examine multiboot data and return highest address used by it
 * Adjust addresses so it points to kernel space
 */
void* multiboot_init(struct multiboot_info* mi)
{
    /* Init data */
    mi = (struct multiboot_info*)((char*)mi + KERNEL_BASE_ADDR);
    multiboot_info = mi;
    unsigned char* end = (unsigned char*)mi + sizeof(struct multiboot_info);

    /* Adjust init_mi address */
    trace("multiboot_info: %p", mi);

    if(mi->flags & MULTIBOOT_FLAG_CMDLINE) {
        mi->cmdline += KERNEL_BASE_ADDR;

        if((unsigned char*)mi->cmdline + strlen(mi->cmdline) + 1 > end)
            end = (unsigned char*)mi->cmdline + strlen(mi->cmdline) + 1;

        trace("cmdline: %s", mi->cmdline);
    }

    if(mi->flags & MULTIBOOT_FLAG_MODINFO) {
        mi->mods_addr = (struct multiboot_mod_entry*)
            ((char*)mi->mods_addr + KERNEL_BASE_ADDR);
        trace("mods_addr: %p", mi->mods_addr);

        if((unsigned char*)(mi->mods_addr + mi->mods_count) > end)
            end = (unsigned char*)(mi->mods_addr + mi->mods_count);

        for(int i = 0; i < mi->mods_count; i++) {
            struct multiboot_mod_entry* entry = mi->mods_addr + i;

            unsigned char* entry_start = entry->start + KERNEL_BASE_ADDR;
            unsigned char* entry_end = entry->end + KERNEL_BASE_ADDR;
            unsigned entry_size = entry_end - entry_start;
            trace("mods[%d]: %p - %p (%d bytes)",
                  i,
                  entry_start,
                  entry_end,
                  entry_size);
            entry->start = entry_start;
            entry->end = entry_end;

            if(entry->str) {
                entry->str = entry->str + KERNEL_BASE_ADDR;
            }

            if(entry_end > end)
                end = entry_end;
            if(entry->str && (unsigned char*)entry->str + strlen(entry->str) + 1 > end)
                end = (unsigned char*)entry->str + strlen(entry->str) + 1;
        }
    }

    if(mi->flags & MULTIBOOT_FLAG_SYMBOLS2) {
        mi->sym2.addr = (char*)mi->sym2.addr + KERNEL_BASE_ADDR;
        trace("mi->sym2.addr: %p (%d bytes)",
              mi->sym2.addr,
              mi->sym2.size * mi->sym2.num);

        if((unsigned char*)mi->sym2.addr + (mi->sym2.size * mi->sym2.num) > end)
            end = (unsigned char*)mi->sym2.addr + (mi->sym2.size * mi->sym2.num);

        elf32_shdr_t* shdrs = mi->sym2.addr;
        elf32_shdr_t* shstr_hdr = shdrs + mi->sym2.shndx;
        const char* section_names = (const char*)shstr_hdr->sh_addr + KERNEL_BASE_ADDR;

        for(int i = 0; i < mi->sym2.num; i++) {
            elf32_shdr_t* shdr = shdrs + i;
            shdr->sh_addr += KERNEL_BASE_ADDR;
            trace("shdrs[%d]: %s %p (%d bytes)",
                  i,
                  section_names + shdr->sh_name,
                  shdr->sh_addr,
                  shdr->sh_size);

            if((unsigned char*)shdr->sh_addr + shdr->sh_size > end)
                end = (unsigned char*)shdr->sh_addr + shdr->sh_size;
        }
    }

    if(mi->flags & MULTIBOOT_FLAG_MMAP) {
        mi->mmap_addr = (struct multiboot_mmap_entry*)
            ((char*)mi->mmap_addr + KERNEL_BASE_ADDR);

        trace("mi->mmap_addr: %p (%d bytes)",
              mi->mmap_addr,
              mi->mmap_len);

        if((unsigned char*)mi->mmap_addr + mi->mmap_len > end)
            end = (unsigned char*)mi->mmap_addr + mi->mmap_len;
    }

    return end;
}

void multiboot_dump()
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
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_MMAP) {
        strlcat(multiboot_flags, "MMAP ", sizeof(multiboot_flags));
        trace("Memory map len: %d", multiboot_info->mmap_len);
        trace("Memory map load address: %p", multiboot_info->mmap_addr);
    }

    trace("Multiboot flags: %s", multiboot_flags);
    trace("_KERNEL_END_: %p", _KERNEL_END_);
}

const struct multiboot_info* multiboot_get_info()
{
    return multiboot_info;
}




