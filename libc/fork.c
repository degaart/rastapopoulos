#include <unistd.h>
#include <rasta.h>
#include <syscall.h>

pid_t fork() {
    uint32_t ret = rs_syscall(SYSCALL_FORK, 0, 0, 0);
    return (pid_t)ret;
}
