#pragma once

#define SYSCALL_GETTICKS    0
#define SYSCALL_SLEEP       1
#define SYSCALL_KEYDOWN     2
#define SYSCALL_EXIT        3
#define SYSCALL_EXIT_THREAD 4

#ifdef RASTA_KERNEL
void syscall_init(void);
#endif

