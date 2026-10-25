#pragma once

#define SYSCALL_GETTICKS      0
#define SYSCALL_SLEEP         1
#define SYSCALL_EXIT          3
#define SYSCALL_EXIT_THREAD   4
#define SYSCALL_GET_KBD_EVENT 5
#define SYSCALL_WRITE         6
#define SYSCALL_EXEC          7
#define SYSCALL_SHUTDOWN      8
#define SYSCALL_LIST          9
#define SYSCALL_LOCK_SCREEN   10
#define SYSCALL_UNLOCK_SCREEN 11
#define SYSCALL_PUTPIXEL      12
#define SYSCALL_TRACE         13

#ifdef RASTA_KERNEL
void syscall_init(void);
#endif

