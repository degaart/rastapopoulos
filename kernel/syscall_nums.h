#ifndef _SYSCALL_NUMS_H_
#define _SYSCALL_NUMS_H_

#define SYSCALL_PORT_OPEN       0x01
#define SYSCALL_PORT_CLOSE      0x02
#define SYSCALL_PORT_SEND       0x03
#define SYSCALL_PORT_READ       0x04
#define SYSCALL_FORK            0x05

#define SYSCALL_HALT            0xFFF0
#define SYSCALL_TRACE           0xFFF1
#define SYSCALL_YIELD           0xFFF2
#define SYSCALL_EXIT            0xFFF3
#define SYSCALL_MMAP            0xFFF4
#define SYSCALL_OUTB            0xFFF5
#define SYSCALL_INB             0xFFF6
#define SYSCALL_OUTW            0xFFF7
#define SYSCALL_INW             0xFFF8

#endif //_SYSCALL_NUMS_H_

