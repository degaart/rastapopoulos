#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../kernel.h"
#include "../util.h"
#include "../kmalloc.h"
#include "../list.h"
#include "../multiboot.h"
#include "../debug.h"
#include "../string.h"

struct initrd_file {
    char name[64];
    void* data;
    unsigned size;
    list_declare_node(initrd_file) node;
};
list_declare(initrd, initrd_file);

static void initrd_init(struct initrd* initrd, 
                        const struct multiboot_info* mi)
{
    if(mi->flags & MULTIBOOT_FLAG_MODINFO) {
        trace("Loading initrd");
        const struct multiboot_mod_entry* modules =
            (struct multiboot_mod_entry*)mi->mods_addr;
        for(int i = 0; i < mi->mods_count; i++) {
            const struct multiboot_mod_entry* mod = modules + i;

            trace("\tmod[%d] %s: %p-%p (%d bytes)",
                  i,
                  mod->str ? mod->str : "",
                  mod->start,
                  mod->end,
                  mod->end - mod->start);

            struct initrd_file* file = kmalloc(sizeof(struct initrd_file));
            bzero(file, sizeof(struct initrd_file));
            strlcpy(file->name, mod->str ? mod->str : "", sizeof(file->name));
            file->size = (unsigned)(mod->end - mod->start);
            file->data = kmalloc(file->size);
            memcpy(file->data, mod->start, file->size);
            list_append(initrd, file, node);
        }
    }
}

const struct initrd_file* initrd_get_file(const struct initrd* initrd,
                                          const char* name)
{
    const struct initrd_file* result = NULL;
    list_foreach(initrd_file, file, initrd, node) {
        if(!strcmp(file->name, name)) {
            result = file;
            break;
        }
    }
    return result;
}

void test_initrd()
{
    trace("Testing initrd");

    struct initrd initrd = {0};
    initrd_init(&initrd, multiboot_get_info());

    const struct initrd_file* file = initrd_get_file(&initrd,
                                                     "hello");
    assert(file != NULL);

    char* str = kmalloc(file->size + 1);
    memcpy(str, file->data, file->size);
    str[file->size] = '\0';
    trace("File contents: \"%s\"", str);
    kfree(str);
}



