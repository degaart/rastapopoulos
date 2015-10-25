#include <rasta.h>
#include <syscall.h>

void rs_outb(int port, int ch) {
    rs_syscall(SYSCALL_OUTB, port, ch, 0);
}

int rs_inb(int port) {
    return rs_syscall(SYSCALL_INB, port, 0, 0);
}

