#include "initrd.h"
#include "multiboot.h"
#include "string.h"
#include "kmalloc.h"
#include "debug.h"
#include "util.h"
#include "idt.h"
#include "syscall_handler.h"
#include "syscall.h"

struct tar_header {
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag[1];
};

static struct initrd initrd = {0};
static unsigned initrd_size;
static void* initrd_data;

uint32_t syscall_initrd_get_size_handler(struct isr_regs* regs)
{
    return initrd_size;
}

uint32_t syscall_initrd_copy_handler(struct isr_regs* regs)
{
    unsigned char* dest = (unsigned char*)regs->ebx;
    uint32_t size = (uint32_t)regs->ecx;

    if(size < initrd_size)
        return -1;

    memcpy(dest, initrd_data, initrd_size);
    return initrd_size;
}



static unsigned getsize(const char *in)
{
    unsigned int size = 0;
    unsigned int j;
    unsigned int count = 1;

    for (j = 11; j > 0; j--, count *= 8)
        size += ((in[j - 1] - '0') * count);

    return size;
}

void initrd_init(const struct multiboot_info* mi)
{
    if(mi->flags & MULTIBOOT_FLAG_MODINFO) {
        trace("Loading initrd");

        if(mi->mods_count > 0) {
            const struct multiboot_mod_entry* mod =
                (struct multiboot_mod_entry*)mi->mods_addr;

            initrd_size = mod->end - mod->start;
            initrd_data = kmalloc(initrd_size);

            trace("Allocating %d bytes for initrd", initrd_size);
            memcpy(initrd_data, mod->start, initrd_size);

            struct tar_header* hdr = (struct tar_header*)mod->start;
            while(true) {
                if(hdr->filename[0] == 0)
                    break;

                unsigned size = getsize(hdr->size);

                struct initrd_file* file = kmalloc(sizeof(struct initrd_file));
                bzero(file, sizeof(struct initrd_file));
                strlcpy(file->name, hdr->filename, sizeof(file->name));
                file->size = size;
                file->data = kmalloc(size);
                memcpy(file->data, (unsigned char*)hdr + 512, size);
                list_append(&initrd, file, node);

                hdr = (struct tar_header*)((unsigned char*)hdr + 512 + ALIGN(size, 512));
            }
        }
    }
}

const struct initrd_file* initrd_get_file(const char* name)
{
    const struct initrd_file* result = NULL;
    list_foreach(initrd_file, file, &initrd, node) {
        if(!strcmp(file->name, name)) {
            result = file;
            break;
        }
    }
    return result;
}

size_t initrd_get_size()
{
    return initrd_size;
}

int initrd_read(void* buffer, size_t size, size_t offset)
{
    if(offset > initrd_size)
        return -1;

    size_t rem = initrd_size - offset;
    if(!rem)
        return 0;

    if(rem < size)
        size = rem;

    memcpy(buffer, (unsigned char*)initrd_data + offset, size);
    return size;
}




