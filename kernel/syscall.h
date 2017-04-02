#pragma once

#include "idt.h"

#include <stdint.h>
#include <stddef.h>

#define SYSCALL_PORTOPEN    0
#define SYSCALL_MSGSEND     1
#define SYSCALL_MSGRECV     2
#define SYSCALL_MSGWAIT     3
#define SYSCALL_MSGPEEK     4
#define SYSCALL_YIELD       5
#define SYSCALL_FORK        6
#define SYSCALL_SETNAME     7
#define SYSCALL_EXIT        8
#define SYSCALL_SLEEP       9

typedef uint32_t (*syscall_handler_t)(struct isr_regs* regs);

extern uint32_t syscall(uint32_t eax, uint32_t ebx,
                        uint32_t ecx, uint32_t edx,
                        uint32_t esi, uint32_t edi);

void syscall_register(unsigned num, syscall_handler_t handler);
void syscall_init();



