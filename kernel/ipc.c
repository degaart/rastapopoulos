#include "ipc.h"
#include "syscall_handler.h"
#include "syscall.h"
#include "kernel.h"
#include "idt.h"
#include "kmalloc.h"
#include "string.h"
#include "scheduler.h"
#include "util.h"
#include "registers.h"

static struct port_list port_list = {0};
static spinlock_t port_list_lock = SPINLOCK_INIT;
static uint32_t reserved_ports = 0;                 /* Reserved ports bitmask */
static int next_port_value = 32; 

/* Get port from its number */
static struct port* port_get(int number)
{
    struct port* port = NULL;

    if(number >= 0) {
        enter_critical_section();
        checked_lock(&port_list_lock);

        list_foreach(port, check, &port_list, node) {
            if(check->number == number) {
                port = check;
                break;
            }
        }
        checked_unlock(&port_list_lock);
        leave_critical_section();
    }
    return port;
}

/*
 * Open a new port and set receiver to current process
 * Params:
 *  ebx             Requested port number. Must be < 32 or -1
 *                  If != -1: Request to open specified port
 *                  Else: Dynamically allocate new port number and return it
 * Returns:
 *  < 0             Error opening port
 *  >= 0            Port number
 */
static uint32_t syscall_portopen_handler(struct isr_regs* regs)
{
    int port_number = regs->ebx;

    if(port_number == INVALID_PORT) {
        checked_lock(&port_list_lock);
        port_number = next_port_value++;
        checked_unlock(&port_list_lock);
    } else {
        checked_lock(&port_list_lock);
        if(BITTEST(reserved_ports, port_number)) {
            port_number = INVALID_PORT;                           /* Already reserved */
        } else {
            BITSET(reserved_ports, port_number);
        }
        checked_unlock(&port_list_lock);
    }

    if(port_number >= 0) {
        struct port* result = kmalloc(sizeof(struct port));
        bzero(result, sizeof(struct port));

        enter_critical_section();
        result->number = port_number;
        result->receiver = current_task_pid();
        result->lock = SPINLOCK_INIT;
        list_init(&result->queue);

        checked_lock(&port_list_lock);
        list_append(&port_list, result, node);
        checked_unlock(&port_list_lock);

        leave_critical_section();
    }

    return port_number;
}

/*
 * Send message to a port
 * Params:
 *  ebx port
 *  ecx message
 * Returns
 *  0   Error
 *  <>0 Success
 */
static uint32_t syscall_msgsend_handler(struct isr_regs* regs)
{
    int port_number = regs->ebx;
    struct message* msg = (struct message*)regs->ecx;

    /* Validate message */
    unsigned checksum = message_checksum(msg);
    assert(checksum == msg->checksum);

    /* Wait for port to be open */
    struct port* port = NULL;
    while(!(port = port_get(port_number))) {
        task_block(INVALID_PORT, port_number, SLEEP_INFINITE);
    }
    
    /* Copy message into kernel space */
    size_t bufsize = sizeof(struct message) + msg->len;
    kernel_heap_check();

    struct message* msg_copy = kmalloc(bufsize);
    kernel_heap_check();

    memcpy(msg_copy, msg, bufsize);
    list_next(msg_copy, node) = NULL;
    list_prev(msg_copy, node) = NULL;
    kernel_heap_check();

    assert(msg_copy->checksum == checksum);

    msg_copy->sender = current_task_pid();
    msg_copy->checksum = message_checksum(msg_copy);
    kernel_heap_check();

    /* Add message to port's queue */
    checked_lock(&port->lock);
    list_append(&port->queue, msg_copy, node);
    checked_unlock(&port->lock);
    kernel_heap_check();

    /* Wake receiver */
    task_wake(port->receiver);

    /* Block ourselves, receiver will wake us when it has successfully called msgrecv() on our message */
    task_block(INVALID_PORT, INVALID_PORT, SLEEP_INFINITE);

    return 1;
}

/*
 * Reads a message from a port
 * Blocks if port's queue is empty
 * Params:
 *  ebx:    port number
 *  ecx:    buffer to put message contents into
 *  edx:    buffer size
 *  esi:    pointer to uint32_t* to receive required buffer size into
 * Returns:
 *  0       no error
 *  1       invalid port number
 *  2       current process cannot read from specified port
 *  3       insufficient buffer size
 */
static uint32_t syscall_msgrecv_handler(struct isr_regs* regs)
{
    assert(!interrupts_enabled());

    int port_number = regs->ebx;
    struct message* buffer = (struct message*)regs->ecx;
    uint32_t buffer_size = regs->edx;
    uint32_t* outsize = (uint32_t*)regs->esi;

    struct port* port = port_get(port_number);
    if(!port)
        return 1;

    if(current_task_pid() != port->receiver)
        return 2;

    while(true) {
        checked_lock(&port->lock);
        bool empty = list_empty(&port->queue);
        checked_unlock(&port->lock);

        if(!empty)
            break;

        wake_tasks_waiting_for_port(port_number);
        task_block(port_number, INVALID_PORT, SLEEP_INFINITE);
        assert(!interrupts_enabled());
        kernel_heap_check();
    }

    uint32_t result;

    checked_lock(&port->lock);
    struct message* message = list_head(&port->queue);

    /* Validate message */
    unsigned checksum = message_checksum(message);
    if(checksum != message->checksum) {
        panic("Corrupted message from %d to %d", message->sender, current_task_pid);
    }
    assert(checksum == message->checksum);

    *outsize = sizeof(struct message) + message->len;
    if(buffer_size >= sizeof(struct message) + message->len) {
        memcpy(buffer, message, sizeof(struct message) + message->len);
        list_remove(&port->queue, message, node);

        /* Wake sender */
        task_wake(message->sender);

        /* Free message */
        kfree(message);
        result = 0;
    } else {
        result = 3;
    }
    checked_unlock(&port->lock);

    return result;
}

/*
 * Sleep current process until the specified port has a non-empty queue
 * Params:
 *  ebx:    Port number
 * Returns:
 *  0       Success
 *  -1      Error
 */
static uint32_t syscall_msgwait_handler(struct isr_regs* regs)
{
    assert(!interrupts_enabled());
    int port_number = regs->ebx;

    struct port* port = port_get(port_number);
    if(!port) {
        return (uint32_t)-1;
    }

    if(current_task_pid() != port->receiver)
        return (uint32_t)-1;

    while(true) {
        checked_lock(&port->lock);
        bool empty = list_empty(&port->queue);
        checked_unlock(&port->lock);

        if(!empty)
            break;

        task_block(port_number, INVALID_PORT, SLEEP_INFINITE);
    }
    return 0;
}

/*
 * Check if specified port has pending messages
 * Params:
 *  ebx:    port number
 * Returns:
 *  0       No pending messages
 *  1       Pending messages
 */
static uint32_t syscall_msgpeek_handler(struct isr_regs* regs)
{
    uint32_t result = 0;
    int port_number = regs->ebx;

    struct port* port = port_get(port_number);
    if(port) {
        checked_lock(&port->lock);
        if(current_task_pid() == port->receiver) {
            if(!list_empty(&port->queue))
                result = 1;
        }
        checked_unlock(&port->lock);
    }
    return result;
}

void ipc_init()
{
    syscall_register(SYSCALL_PORTOPEN, syscall_portopen_handler);
    syscall_register(SYSCALL_MSGSEND, syscall_msgsend_handler);
    syscall_register(SYSCALL_MSGRECV, syscall_msgrecv_handler);
    syscall_register(SYSCALL_MSGWAIT, syscall_msgwait_handler);
    syscall_register(SYSCALL_MSGPEEK, syscall_msgpeek_handler);

    list_init(&port_list);
}


