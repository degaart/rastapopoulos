#include "kernel_task.h"
#include "process.h"
#include "io.h"
#include "util.h"

extern "C" uint32_t call_int81(void);

/*
    Child1 generates prime numbers
    Parent prints them
    Child2 is dumb and only prints stars
*/
void KernelTask::entry() {
    uint32_t pid = call_int81();
    if(!pid) {
        child1();
        halt();
    }

    pid = call_int81();
    if(!pid) {
        child2();
        halt();
    }

    /*
        We must disable interrupts each time we call a kernel function,
        as the kernel is not yet interrupt-safe
    */
    uint32_t lock;
    EnterCriticalSection(lock);
    uint32_t port = Process::syscall_port_open(1000, 0, 0);
    LeaveCriticalSection(lock);

    if(port == Process::INVALID_PORT) {
        TRACE("Failed to open port");
        halt();
    }

    uint32_t prime;
    Message_t msg;
    bzero(&msg, sizeof(msg));

    BREAKPOINT();
    do {
        msg.payload_size = sizeof(unsigned);
        msg.payload = &prime;

        EnterCriticalSection(lock);
        uint32_t ret = Process::syscall_port_read(port, (uint32_t)&msg, 0);
        LeaveCriticalSection(lock);
        if(ret)
            PANIC("Failed to read from port %d", port);

        assert(msg.payload_size == sizeof(unsigned));
        TRACE("%u", prime);
    } while(true);

    EnterCriticalSection(lock);
    Process::syscall_port_close(port, 0, 0);
    LeaveCriticalSection(lock);

    halt();
}

void KernelTask::child1() {
    uint32_t lock;
    unsigned i = 3, count, c;
    for (i = 3 ; ;  i++) {
        for ( c = 2 ; c <= i - 1 ; c++ ) {
            if ( (i%c) == 0 )
                break;
        }

        if ( c == i ) {
            Message_t msg;
            bzero(&msg, sizeof(msg));
            msg.payload_size = sizeof(unsigned);
            msg.payload = &i;

            EnterCriticalSection(lock);
            Process::syscall_port_send(1000, (uint32_t)&msg, 0);
            LeaveCriticalSection(lock);
        }
    }    
}

void KernelTask::child2() {
    while(true) {
        yield();
        outb(0xE9, '.');
    }
}

