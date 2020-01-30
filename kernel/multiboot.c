#include "multiboot.h"
#include "util.h"
#include "string.h"
#include "debug.h"
#include "kernel.h"
#include "kmalloc.h"

static struct multiboot_info* multiboot_info;

/*
 * Copy multiboot info into allocated heap memory
 */
void multiboot_init(const struct multiboot_info* mi)
{
    multiboot_info = kmalloc(sizeof(struct multiboot_info));
    memcpy(multiboot_info, mi, sizeof(struct multiboot_info));
    multiboot_info->cmdline = kmalloc(strlen(mi->cmdline) + 1);
    memcpy(multiboot_info->cmdline, mi->cmdline, strlen(mi->cmdline) + 1);

    multiboot_info->mmap_addr = kmalloc(multiboot_info->mmap_len);
    for(size_t i = 0; i < mi->mmap_len/sizeof(struct multiboot_mmap_entry); i++) {
        memcpy(&multiboot_info->mmap_addr[i], &mi->mmap_addr[i], sizeof(struct multiboot_mmap_entry));
    }
}

const struct multiboot_mmap_entry* multiboot_get_mmap(int* count)
{
    *count = multiboot_info->mmap_len / sizeof(struct multiboot_mmap_entry);
    return multiboot_info->mmap_addr;
}
