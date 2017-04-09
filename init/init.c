#include "syscalls.h"
#include "io.h"
#include "string.h"
#include "debug.h"
#include "port.h"

#define LoggerPort              1
#define LoggerMessageTrace      0
#define LoggerMessageTraceAck   1

/****************************************************************************
 * usermode runtime helpers
 ****************************************************************************/
static void trace_callback(int ch, void* args)
{
    outb(0xE9, ch);
}

void __log(const char* func, const char* file, int line, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    format(trace_callback, NULL, "[%s:%d][%s][init] ", file, line, func);
    formatv(trace_callback, NULL, fmt, ap);
    trace_callback('\n', NULL);

    va_end(ap);
}

void __assertion_failed(const char* function, const char* file, int line, const char* expression)
{
    __log(function, file, line, "Assertion failed: %s", expression);
    abort();
}

static void reboot()
{
    syscall(SYSCALL_REBOOT,
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

/****************************************************************************
 * usermode syscall helpers
 ****************************************************************************/
static void yield()
{
    syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
}

static int fork()
{
    int pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
    return pid;
}

static void user_trace(int ack_port, const char* format, ...)
{
    va_list args;

    unsigned char buffer[128];
    struct message* msg = (struct message*)buffer;

    va_start(args, format);
    vsnprintf((char*)msg->data, sizeof(buffer) - sizeof(struct message), format, args);
    va_end(args);

    msg->sender = 0;
    msg->reply_port = ack_port;
    msg->code = LoggerMessageTrace;
    msg->len = strlen((const char*)msg->data) + 1;
    msg->checksum = message_checksum(msg);

    bool ret = msgsend(LoggerPort, msg);
    assert(ret);

    /* Wait ack */
    unsigned outsize;
    unsigned recv_ret = msgrecv(ack_port, msg, sizeof(buffer), &outsize);
    assert(recv_ret == 0);

    assert(msg->checksum == message_checksum(msg));
    assert(msg->code == LoggerMessageTraceAck);
}

static void user_sleep(unsigned ms)
{
    syscall(SYSCALL_SLEEP,
            ms,
            0,
            0,
            0,
            0);
}

static void user_exit()
{
    syscall(SYSCALL_EXIT,
            0,
            0,
            0,
            0,
            0);
}

static void send_ack(int port, unsigned code, uint32_t result)
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

static void setname(const char* new_name)
{
    syscall(SYSCALL_SETNAME, (uint32_t)new_name, 0, 0, 0, 0);
}

/****************************************************************************
 * test usermode programs
 ****************************************************************************/
static unsigned fibonacci(unsigned n)
{
    unsigned result;

    if (n == 0)
        result = 0;
    else if (n == 1)
        result = 1;
    else
        result = fibonacci(n - 1) + fibonacci(n - 2);

    return result;
} 

static unsigned is_prime(unsigned num)
{
    if(num == 1)
        return 1;

    for(unsigned i = 2; i < num; i++) {
        if(!(num % i))
            return 0;
    }

    return 1;
}

static void fibonacci_entry()
{
    const char* proc_name = "fibonacci";
    setname(proc_name);

    int ack_port = port_open(-1);

    for(unsigned i = 0; i < 37; i++) {
        unsigned fib = fibonacci(i);
        user_trace(ack_port, "fib(%d): %d", i, fib);
    }

    user_trace(ack_port, "fib: Done");
    user_exit();
    invalid_code_path();
}

static void sleeper_entry()
{
    const char* proc_name = "sleeper";
    setname(proc_name);

    int ack_port = port_open(-1);

    for(unsigned i = 0; i < 20; i++) {
        user_sleep(1000);
        user_trace(ack_port, "sleeper: %d", i);
    }

    user_trace(ack_port, "sleeper: Done");

    user_exit();
    invalid_code_path();
}

static void logger_entry()
{
    const char* proc_name = "logger";
    setname(proc_name);

    int ret = port_open(LoggerPort);
    if(ret < 0) {
        panic("Failed to open logger port");
        while(1);
    }

    unsigned char buffer[512];
    struct message* msg = (struct message*)buffer;
    while(1) {
        unsigned outsiz;
        unsigned ret = msgrecv(LoggerPort, 
                               msg, 
                               sizeof(buffer), 
                               &outsiz);
        if(ret != 0) {
            panic("msgrecv failed");
        }

        if(msg->code == LoggerMessageTrace) {
            trace("%s", msg->data);
            send_ack(msg->reply_port, LoggerMessageTraceAck, 0);
        } else {
            panic("Invalid message code");
        }
    }
}

void main()
{
    int logger_pid = fork();
    if(!logger_pid) {
        logger_entry();
        invalid_code_path();
    }

    const char* proc_name = "primes";
    setname(proc_name);

    unsigned start_val = 5001000;

    /* Fork into other tasks */
    int pid = fork();
    if(!pid) {
        start_val = 5002000;
    } else {
        pid = fork();
        if(!pid) {
            start_val = 5003000;
        } else {
            pid = fork();
            if(!pid) {
                fibonacci_entry();
                goto exit;
            } else {
                pid = fork();
                if(!pid) {
                    sleeper_entry();
                    goto exit;
                }
            }
        }
    }

    int ack_port = port_open(-1);

    unsigned end_val = start_val + 500;
    for(unsigned i = start_val; i < end_val; i++) {
        if(is_prime(i)) {
            user_trace(ack_port, "prime: %d", i);
        }
    }

exit:
    user_trace(ack_port, "primes: Done");
    user_exit();
    invalid_code_path();
}

