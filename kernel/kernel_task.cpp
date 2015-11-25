#include "kernel_task.h"
#include "process.h"
#include "io.h"
#include "util.h"
#include "syscall.h"
#include "kernel_msg.h"
#include "timer.h"
#include "kmalloc.h"

#define KERNEL_TASK_PORT        1
#define CHILD1_PORT             2
#define CHILD2_PORT             3
#define INVALID_PORT            0

static const char* errmsg(uint32_t code) {
    switch(code) {
        case Port::SUCCESS:
            return "SUCCESS";
        case Port::INVALID_MESSAGE:
            return "INVALID_MESSAGE";
        case Port::BUFFER_TOO_SMALL:
            return "BUFFER_TOO_SMALL";
        case Port::EMPTY_PORT:
            return "EMPTY_PORT";
        default:
            return "UNKNOWN_ERROR";
    }
}

static uint32_t port_open(uint32_t port) {
    uint32_t ret = Syscall::syscall(SYSCALL_PORT_OPEN, port);
    if(ret == INVALID_PORT)
        PANIC("Failed to open port %u: %s", port, errmsg(ret));
    return ret;
}

static void port_close(uint32_t port) {
    int ret = Syscall::syscall(SYSCALL_PORT_CLOSE, port);
    if(ret)
        PANIC("Failed to close port %u: %s", port, errmsg(ret));
}

static void port_send(uint32_t port, const Message_t* msg) {
    int ret = Syscall::syscall(SYSCALL_PORT_SEND, port, (uint32_t)msg);
    if(ret)
        PANIC("Failed to send message to port %u: %s", port, errmsg(ret));
}

static void port_read(uint32_t port, const Message_t* msg) {
    uint32_t bufsize = msg->payload_size;
    int ret = Syscall::syscall(SYSCALL_PORT_READ, port, (uint32_t)msg);
    if(ret) {
        if(ret == Port::BUFFER_TOO_SMALL)
            PANIC("Failed to read message from port %u: %s. Need %u bytes, has %u bytes", port, errmsg(ret), msg->payload_size, bufsize);

        PANIC("Failed to read message from port %u: %s", port, errmsg(ret));
    }
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
    //TRACE("%d %s %s", sender->pid(), sender->name(), msg);
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
    uint32_t port = port_open(KERNEL_TASK_PORT);

    Message_t msg;
    bzero(&msg, sizeof(msg));
    msg.payload = kmalloc(4096);
    msg.payload_size = 4096;

    do {
        msg.payload_size = 4096;
        port_read(port, &msg);

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

            port_send(KERNEL_TASK_PORT, &msg);
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
    port_read(result_port, &msg);

    return result;
}

void KernelTask::child2() {
    uint32_t port = port_open(CHILD2_PORT);

    while(true) {
        for(unsigned i=0; i < 0x1000000; i++)
            ;
        
        uint64_t ticks_start = get_ticks(port);
        sleep(1000);
        uint64_t ticks_end = get_ticks(port);

        TRACE("\nSleep duration: %u", (uint32_t)(ticks_end - ticks_start));
    }

    port_close(CHILD2_PORT);
}

