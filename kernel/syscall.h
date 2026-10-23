#pragma once

#define SYSCALL_GETTICKS      0
#define SYSCALL_SLEEP         1
#define SYSCALL_EXIT          3
#define SYSCALL_EXIT_THREAD   4
#define SYSCALL_GET_KBD_EVENT 5
#define SYSCALL_WRITE         6
#define SYSCALL_EXEC          7
#define SYSCALL_SHUTDOWN      8

#ifdef RASTA_KERNEL
void syscall_init(void);
#endif

