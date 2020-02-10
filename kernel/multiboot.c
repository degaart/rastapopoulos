#include "multiboot.h"
#include "util.h"
#include "string.h"
#include "debug.h"
#include "kernel.h"
#include "kmalloc.h"
#include "elf.h"
#include "string.h"

static struct multiboot_info* multiboot_info;

/*
 * - Recreate a fixed multiboot_info struct
 * - Put recreated data after the highest address used by original multiboot structure so as to not overwrite it
 * - Return highest used address to initialize kernel heap
 * - Fix addressed to point to higher half
 */
const unsigned char* multiboot_init(const struct multiboot_info* mi)
{
    /* Copy into static memory */
    mi = (const struct multiboot_info*)((unsigned char*)mi + KERNEL_BASE);
    trace("Initializing multiboot info from %p", mi);
    const unsigned char* end = (unsigned char*)mi + sizeof(struct multiboot_info);

    /* then calculate highest used address */
    if(mi->flags & MULTIBOOT_FLAG_CMDLINE) {
        const char* cmdline = mi->cmdline + KERNEL_BASE;
        if((unsigned char*)cmdline + strlen(cmdline) + 1 > end)
            end = (unsigned char*)cmdline + strlen(cmdline) + 1;
    }

    if((mi->flags & MULTIBOOT_FLAG_MODINFO) && mi->mods_count) {
        const struct multiboot_mod_entry* mods_addr = 
            (struct multiboot_mod_entry*)((unsigned char*)mi->mods_addr + KERNEL_BASE);

        /* We only support one module */
        if((unsigned char*)mods_addr > end)
            end = (unsigned char*)mods_addr;

        const struct multiboot_mod_entry* entry = mods_addr;
        const unsigned char* entry_start = entry->start + KERNEL_BASE;
        const unsigned char* entry_end = entry->end + KERNEL_BASE;
        size_t entry_size = entry_end - entry_start;
        const char* entry_str = entry->str + KERNEL_BASE;

        if(entry_end > end)
            end = (unsigned char*)entry_end;
        if((unsigned char*)entry_str + strlen(entry_str) + 1 > end)
            end = (unsigned char*)entry_str + strlen(entry_str);
    }

    if(mi->flags & MULTIBOOT_FLAG_SYMBOLS2) {
        const void* sym2_addr = (unsigned char*)mi->sym2.addr + KERNEL_BASE;
        if((unsigned char*)sym2_addr > end)
            end = sym2_addr;

        /* TODO: Take care not to overwrite the content of these sections while freeing unused memory */
        if((unsigned char*)mi->sym2.addr + (mi->sym2.size * mi->sym2.num) > end)
            end = (unsigned char*)mi->sym2.addr + (mi->sym2.size * mi->sym2.num);

        const elf32_shdr_t* shdrs = sym2_addr;
        const elf32_shdr_t* shstr_hdr = shdrs + mi->sym2.shndx;
        const char* section_names = (const char*)shstr_hdr->sh_addr + KERNEL_BASE;

        for(size_t i = 0; i < mi->sym2.num; i++) {
            const elf32_shdr_t* shdr = shdrs + i;
            const unsigned char* shdr_addr = (unsigned char*)shdr->sh_addr + KERNEL_BASE;
            if(shdr_addr + shdr->sh_size > end)
                end = shdr_addr + shdr->sh_size;
        }
    }

    if(mi->flags & MULTIBOOT_FLAG_MMAP) {
        const struct multiboot_mmap_entry* mmap_addr =
            (const struct multiboot_mmap_entry*)((unsigned char*)mi->mmap_addr + KERNEL_BASE);
        if((unsigned char*)mmap_addr + mi->mmap_len > end)
            end = (unsigned char*)mmap_addr + mi->mmap_len;
    }

    return end;
}

void multiboot_fix(const struct multiboot_info* mi)
{
    trace("Fixing multiboot info");

    mi = (const struct multiboot_info*)((unsigned char*)mi + KERNEL_BASE);
    multiboot_info = kmalloc(sizeof(struct multiboot_info));
    memcpy(multiboot_info, mi + KERNEL_BASE, sizeof(struct multiboot_info));
    if(mi->flags & MULTIBOOT_FLAG_CMDLINE) {
        size_t len = strlen(mi->cmdline + KERNEL_BASE);
        multiboot_info->cmdline = kmalloc(len + 1);
        memcpy(multiboot_info->cmdline, mi->cmdline + KERNEL_BASE, len + 1);
    }

    if((mi->flags & MULTIBOOT_FLAG_MODINFO) && mi->mods_count) {
        /* we only support one module */
        multiboot_info->mods_count = 1;
        multiboot_info->mods_addr = (struct multiboot_mod_entry*)((unsigned char*)mi->mods_addr + KERNEL_BASE);

        struct multiboot_mod_entry* entry = multiboot_info->mods_addr;
        const unsigned char* entry_start = entry->start + KERNEL_BASE;
        const unsigned char* entry_end = entry->end + KERNEL_BASE;
        size_t entry_size = entry_end - entry_start;

        entry->start = kmalloc(entry_size);
        entry->end = entry->start + entry_size;
        entry->str = "";
        memcpy(entry->start, entry_start, entry_size);
    }

    if(mi->flags & MULTIBOOT_FLAG_SYMBOLS2) {
        multiboot_info->sym2.addr = (unsigned char*)multiboot_info->sym2.addr + KERNEL_BASE;

        elf32_shdr_t* shdrs = multiboot_info->sym2.addr;
        for(size_t i = 0; i < mi->sym2.num; i++) {
            elf32_shdr_t* shdr = shdrs + i;
            shdr->sh_addr += KERNEL_BASE;
        }
    }

    if(mi->flags & MULTIBOOT_FLAG_MMAP) {
        multiboot_info->mmap_addr = (struct multiboot_mmap_entry*)((unsigned char*)mi->mmap_addr + KERNEL_BASE);
    }
}

const struct multiboot_mmap_entry* multiboot_get_mmap(int* count)
{
    *count = multiboot_info->mmap_len / sizeof(struct multiboot_mmap_entry);
    return multiboot_info->mmap_addr;
}

const void* multiboot_get_initrd(size_t* size)
{
    if((multiboot_info->flags & MULTIBOOT_FLAG_MODINFO) && multiboot_info->mods_count) {
        const struct multiboot_mod_entry* mod = (struct multiboot_mod_entry*)multiboot_info->mods_addr;
        size_t initrd_size = mod->end - mod->start;
        const void* initrd_data = mod->start;
        *size = initrd_size;
        return initrd_data;
    }
    return NULL;
}


