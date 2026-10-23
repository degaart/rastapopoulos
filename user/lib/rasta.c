#include "rasta.h"
#include "../../kernel/syscall.h"

uint64_t sys_getticks(void)
{
    int64_t ticks = syscall64(SYSCALL_GETTICKS, 0, 0, 0, 0, 0);
    return ticks;
}

void sys_sleep(uint32_t ticks)
{
    syscall(SYSCALL_SLEEP, ticks, 0, 0, 0, 0);
}

void sys_exit(uint8_t status)
{
    syscall(SYSCALL_EXIT, status, 0, 0, 0, 0);
    __builtin_unreachable();
}

void sys_exit_thread(void)
{
    syscall(SYSCALL_EXIT_THREAD, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

int sys_get_kbd_event(struct KeyEvent* evt, int* overflow, int block)
{
    return syscall(SYSCALL_GET_KBD_EVENT, (uint32_t)evt, (uintptr_t)overflow,
                   block, 0, 0);
}

int sys_write(const char* s, size_t len)
{
    return syscall(SYSCALL_WRITE, (uintptr_t)s, len, 0, 0, 0);
}

int sys_exec(const char* command, const char* arguments)
{
    return syscall(SYSCALL_EXEC, (uintptr_t)command, (uintptr_t)arguments, 0,
                   0, 0);
}

int sys_shutdown(void)
{
    return syscall(SYSCALL_SHUTDOWN, 0, 0, 0, 0, 0);
}

