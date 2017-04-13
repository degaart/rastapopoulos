#pragma once

#include "idt.h"

#include <stdint.h>
#include <stddef.h>

typedef uint32_t (*syscall_handler_t)(struct isr_regs* regs);

void syscall_register(unsigned num, syscall_handler_t handler);
void syscall_init();



