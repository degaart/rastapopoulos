#pragma once

#define SYSCALL_GETTICKS           0
#define SYSCALL_SLEEP              1
#define SYSCALL_EXIT               3
#define SYSCALL_EXIT_THREAD        4
#define SYSCALL_GET_KBD_EVENT      5
#define SYSCALL_WRITE              6
#define SYSCALL_EXEC               7
#define SYSCALL_SHUTDOWN           8
#define SYSCALL_LIST               9
#define SYSCALL_LOCK_SCREEN        10
#define SYSCALL_UNLOCK_SCREEN      11
#define SYSCALL_PUTPIXEL           12
#define SYSCALL_TRACE              13
#define SYSCALL_VGA_SWAP           14
#define SYSCALL_VGA_FILLRECT       15
#define SYSCALL_VGA_RECT           16
#define SYSCALL_VGA_DRAWLINE       17
#define SYSCALL_VGA_OVAL           18
#define SYSCALL_VGA_ROUNDRECT      19
#define SYSCALL_VGA_BLIT           20
#define SYSCALL_VGA_LOAD_BITMAP    21
#define SYSCALL_VGA_DESTROY_BITMAP 22
#define SYSCALL_VGA_SET_PALETTE    23

#define SYSCALL_PERF_CLOCK    24
#define SYSCALL_VGA_SWAP_RECT 25

#ifdef RASTA_KERNEL
void syscall_init(void);
#endif

