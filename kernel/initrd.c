#include "initrd.h"
#include "debug.h"
#include "kmalloc.h"
#include "string.h"
#include "util.h"

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

static size_t initrd_size;
static const unsigned char* initrd_data;
static TAILQ_HEAD(initrd_files, initrd_file) files;

static unsigned getsize(const char *in)
{
    unsigned int size = 0;
    unsigned int j;
    unsigned int count = 1;

    for (j = 11; j > 0; j--, count *= 8)
        size += ((in[j - 1] - '0') * count);

    return size;
}

void initrd_init(const void* idata, size_t isize)
{
    TAILQ_INIT(&files);

    initrd_size = isize;
    initrd_data = idata;

    const struct tar_header* hdr = (struct tar_header*)initrd_data;
    while(1) {
        if(hdr->filename[0] == 0)
            break;

        size_t size = getsize(hdr->size);
        struct initrd_file* file = kmalloc(sizeof(struct initrd_file));
        bzero(file, sizeof(struct initrd_file));
        strlcpy(file->name, hdr->filename, sizeof(file->name));
        file->size = size;
        file->data = (unsigned char*)hdr + 512;
        TAILQ_INSERT_TAIL(&files, file, next);

        trace("\t%s\t%d", file->name, file->size);
        hdr = (struct tar_header*)((unsigned char*)hdr + 512 + ALIGN(size, 512));
    }
}

const struct initrd_file* initrd_get_file(const char* name)
{
    struct initrd_file* file;
    TAILQ_FOREACH(file, &files, next) {
        if(!strcmp(file->name, name))
            return file;
    }
    return NULL;
}


