#pragma once

#define SYSCALL_GETTICKS 0
#define SYSCALL_SLEEP    1
#define SYSCALL_KEYDOWN  2

#ifdef RASTA_KERNEL
void syscall_init(void);
#endif

