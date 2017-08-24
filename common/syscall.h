#pragma once

#include <stdint.h>

#define SYSCALL_EXIT            0
#define SYSCALL_PORTOPEN        1
#define SYSCALL_MSGSEND         2
#define SYSCALL_MSGRECV         3
#define SYSCALL_MSGWAIT         4
#define SYSCALL_MSGPEEK         5
#define SYSCALL_YIELD           6
#define SYSCALL_FORK            7
#define SYSCALL_SLEEP           9
#define SYSCALL_EXEC            11
#define SYSCALL_MMAP            13
#define SYSCALL_MUNMAP          14

/* put current task into sleeping queue */
#define SYSCALL_BLOCK           17

extern uint32_t syscall(uint32_t eax, uint32_t ebx,
                        uint32_t ecx, uint32_t edx,
                        uint32_t esi, uint32_t edi);

