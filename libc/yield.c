#include <syscall.h>
#include <rasta.h>

void yield() {
    syscall(SYSCALL_YIELD, 0, 0, 0);
}

