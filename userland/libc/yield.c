#include <syscall.h>
#include <rasta.h>

void rs_yield() {
    rs_syscall(SYSCALL_YIELD, 0, 0, 0);
}

