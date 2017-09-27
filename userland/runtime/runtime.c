#include "runtime.h"
#include "syscall.h"
#include "io.h"
#include "string.h"
#include "port.h"
#include "debug.h"
#include "../logger/logger.h"
#include "task_info.h"
#include "../vfs/vfs.h"
#include "../../kernel/kernel_task.h"
#include "serializer.h"
#include <stdarg.h>
#include "malloc.h"
#include "util.h"

struct pcb pcb;
int errno = 0;

void yield()
{
    syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
}

int fork()
{
    int pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
    if(pid == 0) {
        pcb.ack_port = port_open(-1);
    }
    return pid;
}

void sleep(unsigned ms)
{
    syscall(SYSCALL_SLEEP,
            ms,
            0,
            0,
            0,
            0);
}

void exit()
{
    syscall(SYSCALL_EXIT,
            0,
            0,
            0,
            0,
            0);
}

void abort()
{
    reboot();
}

void exec(const char* filename)
{
    syscall(SYSCALL_EXEC,
            (uint32_t)filename,
            0,
            0,
            0,
            0);
    panic("Exec failed");
}

void debug_writen(const char* str, size_t count)
{
    while(count) {
        outb(0xE9, *str);
        str++;
        count--;
    }
}

void debug_write(const char* str)
{
    while(*str) {
        outb(0xE9, *str);
        str++;
    }
}

void __assertion_failed(const char* function, const char* file, int line, const char* expression)
{
    __log(function, file, line, "Assertion failed: %s", expression);
    abort();
}

void* mmap(void* addr, size_t size, uint32_t flags)
{
    uint32_t result = syscall(SYSCALL_MMAP,
                              (uint32_t)addr,
                              (uint32_t)size,
                              flags,
                              0,
                              0);
    return (void*)result;
}

int munmap(void* addr)
{
    uint32_t result = syscall(SYSCALL_MUNMAP,
                              (uint32_t)addr,
                              0,
                              0,
                              0,
                              0);
    return (int)result;
}

static unsigned char* program_break = 0;
void* sbrk(ptrdiff_t incr)
{
    if(!program_break)
        program_break = ALIGN(_END_, 4096);

    if(!incr) {
        return program_break;
    } else if(incr < 0) {
        panic("Not implemented yet");
    } else {
        incr = ALIGN(incr, 4096);
        void* result = mmap(program_break, incr, PROT_READ|PROT_WRITE);
        assert(result != NULL);

        program_break += incr;
        return result;
    }
}

int hwportopen(int port)
{
    int ret = syscall(SYSCALL_HWPORTOPEN,
                      port,
                      0,
                      0,
                      0,
                      0);
    return ret;
}

void main();
void runtime_entry()
{
    pcb.ack_port = port_open(-1);
    main();
    exit();
}



