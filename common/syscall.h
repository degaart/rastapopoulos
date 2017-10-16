#pragma once

#include <stdint.h>

enum SYSCALLS {
    SYSCALL_EXIT = 0,
    SYSCALL_TRACE,
    SYSCALL_PORTOPEN,
    SYSCALL_PORTCHECK,
    SYSCALL_MSGSEND,
    SYSCALL_MSGRECV,
    SYSCALL_MSGWAIT,
    SYSCALL_MSGPEEK,
    SYSCALL_YIELD,
    SYSCALL_FORK,
    SYSCALL_SLEEP,
    SYSCALL_EXEC,
    SYSCALL_MMAP,
    SYSCALL_MUNMAP,
    SYSCALL_HWPORTOPEN,
    SYSCALL_BLOCK,
    SYSCALL_MMAP_PHYS,
};


extern uint32_t syscall(uint32_t eax, uint32_t ebx,
                        uint32_t ecx, uint32_t edx,
                        uint32_t esi, uint32_t edi);

