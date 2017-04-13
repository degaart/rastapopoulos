#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "util.h"
#include "list.h"
#include "string.h"

#include "../kernel.h"
#include "../kmalloc.h"
#include "../multiboot.h"
#include "../kdebug.h"
#include "../initrd.h"

static void test_load_file()
{
    const struct initrd_file* file = initrd_get_file("grub.cfg");
    assert(file != NULL);

    char* str = kmalloc(file->size + 1);
    memcpy(str, file->data, file->size);
    str[file->size] = '\0';
    trace("File contents: \"%s\"", str);
    kfree(str);
}

void test_initrd()
{
    trace("Testing initrd");

    test_load_file();
}



