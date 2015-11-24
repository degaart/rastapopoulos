#include "kernel_task.h"
#include "process.h"
#include "io.h"
#include "util.h"
#include "syscall.h"
#include "kernel_msg.h"
#include "timer.h"

#define KERNEL_TASK_PORT        1
#define CHILD1_PORT             2
#define CHILD2_PORT             3

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

static void sleep(unsigned duration) {
    Syscall::syscall(SYSCALL_YIELD, Process::YIELD_SLEEP, duration);
}

static void msg_trace(Process* sender, const char* msg, size_t size) {
    TRACE("%d %s %s", sender->pid(), sender->name(), msg);
}

static void msg_get_ticks(uint32_t result_port) {
    uint64_t ticks = Timer::current_timestamp();

    Message_t msg;
    bzero(&msg, sizeof(Message_t));
    msg.result = true;
    msg.payload = &ticks;
    msg.payload_size = sizeof(ticks);

    port_send(result_port, &msg);
}

/*
    WARNING:
        Kernel data-structures aren't interrupt safe. This is especially true for
        ANYTHING that uses malloc()!

        So we need to enter a critical section everytime we call a function or access
        a data structure located in another parts of the code. Syscalls are safe, though

        Here are some functions that are interrupt-safe:
            - All functions in debug.cpp (TRACE, etc)
            - All functions in string.cpp except strdup
*/
void KernelTask::entry() {
    uint32_t bkl;

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
        We can't directly call syscall handlers, as some syscalls need to save process context
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

        switch(msg.id) {
            case MSG_TRACE:
                msg_trace(msg.sender, (const char*)msg.payload, msg.payload_size);
                break;
            case MSG_EXIT:
                PANIC("Not implemented yet");
                break;
            // case MSG_SLEEP:
            //     assert(msg.payload_size == sizeof(unsigned));
            //     sleep(msg.sender, *(unsigned*)msg.payload);
            //     break;
            case MSG_EXEC:
                PANIC("Not implemented yet");
                break;
            case MSG_MMAP:
                PANIC("Not implemented yet");
                break;
            case MSG_OUTB:
                PANIC("Not implemented yet");
                break;
            case MSG_INB:
                PANIC("Not implemented yet");
                break;
            case MSG_OUTW:
                PANIC("Not implemented yet");
                break;
            case MSG_INW:
                PANIC("Not implemented yet");
                break;
            case MSG_OUTD:
                PANIC("Not implemented yet");
                break;
            case MSG_IND:
                PANIC("Not implemented yet");
                break;
            case MSG_GETTICKS:
                msg_get_ticks(msg.result_port);
                break;
            default:
                PANIC("Invalid kernel message: %d", msg.id);
                break;
        }
    } while(true);

    port_close(port);
    halt();
}

void KernelTask::child1() {
    uint32_t bkl;

    unsigned i = 3, count, c;
    for (i = 3 ; ;  i++) {
        for ( c = 2 ; c <= i - 1 ; c++ ) {
            if ( (i%c) == 0 )
                break;
        }

        if ( c == i ) {
            char buffer[11];

            EnterCriticalSection(bkl);
            String::itoa(buffer, c);
            LeaveCriticalSection(bkl);

            Message_t msg;
            bzero(&msg, sizeof(msg));
            msg.id = MSG_TRACE;
            msg.payload_size = strlen(buffer);
            msg.payload = buffer;

            port_send(1000, &msg);
        }
    }    
}

static uint64_t get_ticks(uint32_t result_port) {
    Message_t msg;
    bzero(&msg, sizeof(msg));
    msg.id = MSG_GETTICKS;
    msg.result_port = result_port;
    port_send(KERNEL_TASK_PORT, &msg);

    uint64_t result;
    msg.payload = &result;
    msg.payload_size = sizeof(result);
    if(port_read(result_port, &msg)) {
        PANIC("Failed to read from port");
    }

    return result;
}

void KernelTask::child2() {
    uint32_t port = port_open(CHILD2_PORT);
    if(port == Process::INVALID_PORT) {
        PANIC("Failed to open port %d", CHILD2_PORT);
    }


    while(true) {
        for(unsigned i=0; i < 0x1000000; i++)
            ;
        
        // uint64_t ticks_start = get_ticks(port);
        sleep(500);
        // uint64_t ticks_end = get_ticks(port);

        // TRACE("Sleep duration: %u", (uint32_t)(ticks_end - ticks_start));
    }

    port_close(CHILD2_PORT);
}

