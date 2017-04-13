#include "runtime.h"
#include "syscall.h"
#include "io.h"
#include "string.h"
#include "port.h"
#include "debug.h"
#include "../logger/logger.h"
#include "task_info.h"
#include <stdarg.h>

struct pcb pcb;

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

void setname(const char* new_name)
{
    syscall(SYSCALL_SETNAME, (uint32_t)new_name, 0, 0, 0, 0);
}

void abort()
{
    reboot();
}

void reboot()
{
    syscall(SYSCALL_REBOOT,
            0,
            0,
            0,
            0,
            0);
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

void send_ack(int port, unsigned code, uint32_t result)
{
    unsigned char buffer[sizeof(struct message) + sizeof(uint32_t)] = {0};
    struct message* msg = (struct message*)buffer;
    msg->code = code;
    msg->len = sizeof(uint32_t);
    *((uint32_t*)msg->data) = result;
    msg->checksum = message_checksum(msg);

    bool ret = msgsend(port, msg);
    assert(ret);
}

void debug_write(const char* str)
{
    while(*str) {
        outb(0xE9, *str);
        str++;
    }
}

bool get_task_info(int pid, struct task_info* buffer)
{
    uint32_t result = syscall(SYSCALL_TASK_INFO,
                              pid,
                              (uint32_t)buffer,
                              0,
                              0,
                              0);
    return result != 0;
}

void __log(const char* func, const char* file, int line, const char* fmt, ...)
{
    va_list args;

    unsigned char buffer[128];
    struct message* msg = (struct message*)buffer;
    char* msg_text = (char*)msg->data;

    va_start(args, fmt);
    vsnprintf(msg_text, sizeof(buffer) - sizeof(struct message), fmt, args);
    va_end(args);

    msg->sender = 0;
    msg->reply_port = pcb.ack_port;
    msg->code = LoggerMessageTrace;
    msg->len = strlen((const char*)msg->data) + 1;
    msg->checksum = message_checksum(msg);

    bool ret = msgsend(LoggerPort, msg);
    if(!ret) {
        /* Logger must not be up, manually write to log */
        debug_write("Failed to send message to logger\n");
        debug_write("Message: ");
        debug_write(msg_text);
        debug_write("\n");
    } else {
        /* Wait ack */
        unsigned outsize;
        unsigned recv_ret = msgrecv(pcb.ack_port, msg, sizeof(buffer), &outsize);
        assert(recv_ret == 0);

        assert(msg->checksum == message_checksum(msg));
        assert(msg->code == LoggerMessageTraceAck);
    }
}

void __assertion_failed(const char* function, const char* file, int line, const char* expression)
{
    __log(function, file, line, "Assertion failed: %s", expression);
    abort();
}

void main();
void runtime_entry()
{
    pcb.ack_port = port_open(-1);
    main();
    exit();
}



