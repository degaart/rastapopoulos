#include <rasta.h>
#include <syscall.h>

void rs_outb(unsigned port, unsigned ch) {
    rs_syscall(SYSCALL_OUTB, port, ch, 0);
}

unsigned rs_inb(unsigned port) {
    return rs_syscall(SYSCALL_INB, port, 0, 0);
}

void rs_outw(unsigned port, unsigned val) {
    rs_syscall(SYSCALL_OUTW, port, val, 0);
}

unsigned rs_inw(unsigned port) {
    return rs_syscall(SYSCALL_INW, port, 0, 0);
}

