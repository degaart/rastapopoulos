#pragma once

#include <stddef.h>
#include "queue.h"
#include "multiboot.h"

struct initrd_file {
    char name[100];
    void* data;
    unsigned size;
    TAILQ_ENTRY(initrd_file) next;
};

void initrd_init(const void* initrd_data, size_t initrd_size);
const struct initrd_file* initrd_get_file(const char* name);


