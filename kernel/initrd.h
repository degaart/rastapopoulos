#pragma once

#include "list.h"
#include "multiboot.h"
#include <stddef.h>

struct initrd_file {
    char name[100];
    void* data;
    unsigned size;
    list_declare_node(initrd_file) node;
};
list_declare(initrd, initrd_file);

void initrd_init(const struct multiboot_info* mi);
const struct initrd_file* initrd_get_file(const char* name);
size_t initrd_get_size();
int initrd_read(void* buffer, size_t size, size_t offset);


