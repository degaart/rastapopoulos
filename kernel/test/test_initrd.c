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
#include "../initrd.h"

void test_initrd()
{
    trace("Testing initrd");

    const struct initrd_file* file = initrd_get_file("hello");
    assert(file != NULL);

    char* str = kmalloc(file->size + 1);
    memcpy(str, file->data, file->size);
    str[file->size] = '\0';
    trace("File contents: \"%s\"", str);
    kfree(str);
}



