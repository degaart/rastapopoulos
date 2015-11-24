#include "kernel_task.h"
#include "process.h"
#include "io.h"
#include "util.h"
#include "syscall.h"

static uint32_t port_open(uint32_t port) {
    return Syscall::syscall(SYSCALL_PORT_OPEN, port);
}

static uint32_t port_close(uint32_t port) {
    return Syscall::syscall(SYSCALL_PORT_CLOSE, port);
}

static uint32_t port_send(uint32_t port, const Message_t* msg) {
    return Syscall::syscall(SYSCALL_PORT_SEND, port, (uint32_t)msg);
}

static uint32_t port_read(uint32_t port, const Message_t* msg) {
    return Syscall::syscall(SYSCALL_PORT_READ, port, (uint32_t)msg);
}

static uint32_t fork() {
    return Syscall::syscall(SYSCALL_FORK);
}

static uint32_t _yield() {
    return Syscall::syscall(SYSCALL_YIELD);
}

/*
    Child1 generates prime numbers
    Parent prints them
    Child2 is dumb and only prints stars
*/
void KernelTask::entry() {
    uint32_t pid = fork();
    if(!pid) {
        child1();
        halt();
    }

    pid = fork();
    if(!pid) {
        child2();
        halt();
    }

    /*
        We can't directly call syscall handlers, as the kernel is not
        yet interrupt-safe
    */
    uint32_t port = port_open(1000);
    if(port == Process::INVALID_PORT) {
        TRACE("Failed to open port");
        halt();
    }

    uint32_t prime;
    Message_t msg;
    bzero(&msg, sizeof(msg));

    do {
        msg.payload_size = sizeof(unsigned);
        msg.payload = &prime;
        if(port_read(port, &msg)) {
            PANIC("Failed to read from port %d", port);
        }

        assert(msg.payload_size == sizeof(unsigned));
        //outb(0xE9, '*');
    } while(true);

    port_close(port);
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

            port_send(1000, &msg);
        }
    }    
}

void KernelTask::child2() {
    while(true) {
        for(unsigned i=0; i < 0x1000000; i++)
            ;
        //outb(0xE9, '.');
        // _yield();
    }
}

