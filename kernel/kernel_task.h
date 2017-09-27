#pragma once

#include <stdint.h>

struct kernel_initrd_read_args {
    uint32_t size;
    uint32_t offset;
};

void kernel_task_entry();

