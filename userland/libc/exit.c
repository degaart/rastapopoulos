#include <stdlib.h>
#include <syscall.h>

void exit(int status) {
    rs_syscall(SYSCALL_EXIT, status, 0, 0);
}
