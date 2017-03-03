#pragma once

#include "idt.h"

enum Ring {
    Ring0 = 0,
    Ring3 = 3
};

struct process {
    unsigned pid;
    const char* name;
    void* pagedir;
    void* kernel_stack;             /* Start of kernel stack, unshared */
    void* user_stack;               /* Start of user stack, unshared */
    enum Ring current_ring;         /* 0 or 3 */
    void* kernel_esp;
    struct isr_regs registers;
};

