#include "../process.h"
#include "../debug.h"
#include "../util.h"
#include "../pmm.h"
#include "../vmm.h"
#include "../kernel.h"
#include "../string.h"
#include "../gdt.h"
#include "../context.h"
#include "../kmalloc.h"
#include "../registers.h"
#include "../timer.h"
#include "../io.h"
#include "../locks.h"
#include "../list.h"

/*
 * Address-space layout for each task
 *
 *  ---------------------------------------------   0x00000000
 *   shared low kernel space
 *  ---------------------------------------------   0x00400000
 *   process-specific user space
 *  ---------------------------------------------
 *   free address space
 *  ---------------------------------------------   0xBFFFC000
 *   process-specific user stack
 *  ---------------------------------------------   0xBFFFD000
 *   guard page
 *  ---------------------------------------------   0xBFFFE000
 *   process-specific kernel stack
 *  ---------------------------------------------   0xBFFFF000
 *   guard page
 *  ---------------------------------------------   0xC0000000
 *   shared high kernel space
 *  ---------------------------------------------   0xFFFFFFFF
 */

#define USER_STACK          ((unsigned char*)0xBFFFC000)
#define KERNEL_STACK        ((unsigned char*)0xBFFFE000)

/*
 * Port: one-way communication channel between processes
 * Blocking receive and sends
 * Identified by a single, unique number
 * Single receiver, multiple senders
 */
struct port {
    unsigned number;            /* Port number */
    int receiver;
    struct list queue;          /* Message queue */
    spinlock_t lock;
};

struct message {
    uint32_t checksum;
    unsigned sender;            /* Sending process pid */
    unsigned reply_port;        /* Port number to send response to */
    unsigned code;              /* Message code, interpretation depends on receiver */
    size_t len;                 /* Length of data[] */
    unsigned char data[];
};

struct task {
    int pid;
    char name[32];
    struct pagedir* pagedir;
    struct context context;
    unsigned wait_port;
};

static struct list ready_queue = {0};
static spinlock_t ready_queue_lock = SPINLOCK_INIT;

static struct list sleeping_queue = {0};
static spinlock_t sleeping_queue_lock = SPINLOCK_INIT;

static struct list msgwait_queue = {0};
static spinlock_t msgwait_queue_lock = SPINLOCK_INIT;

static struct list port_list = {0};
static spinlock_t port_list_lock = SPINLOCK_INIT;

static struct task* kernel_task = NULL;
static struct task* current_task = NULL;
static struct task* idle_task = NULL;

static int next_pid_value = 0;
static unsigned next_port_value = 1; 

extern uint32_t syscall(uint32_t eax, uint32_t ebx,
                        uint32_t ecx, uint32_t edx,
                        uint32_t esi, uint32_t edi);
static void USERFUNC msgwait(unsigned port);

#define KernelMessageTrace      0
#define KernelMessageTraceAck   1
#define KernelMessageExit       2
#define KernelMessageSleep      3
#define KernelMessageSleepAck   4


/****************************************************************************
 * task management
 ****************************************************************************/

/*
 * Create a new task structure (but does not push it to any queues)
 */
static struct task* task_create(const char* name)
{
    struct task* result = kmalloc(sizeof(struct task));
    bzero(result, sizeof(struct task));

    enter_critical_section();
    result->pid = next_pid_value++;
    leave_critical_section();

    strlcpy(result->name, name, sizeof(result->name));
    result->pagedir = vmm_clone_pagedir();

    return result;
}

/*
 * Get a task by it's pid
 */
struct task_info {
    struct task* task;
    struct list* queue;
    struct list_node* node;
};
static struct task_info task_getinfo(int pid)
{
    struct task_info result = {0};

    enter_critical_section();

    if(pid == 0)
        result.task = kernel_task;
    else if(pid == 1)
        result.task = idle_task;

    if(!result.task) {
        checked_lock(&ready_queue_lock);
        for(struct list_node* n = ready_queue.head; n; n = n->next) {
            struct task* task = n->data;
            if(task->pid == pid) {
                result.task = task;
                result.queue = &ready_queue;
                result.node = n;
                break;
            }
        }
        checked_unlock(&ready_queue_lock);
    }

    if(!result.task) {
        checked_lock(&sleeping_queue_lock);
        for(struct list_node* n = sleeping_queue.head; n; n = n->next) {
            struct task* task = n->data;
            if(task->pid == pid) {
                result.task = task;
                result.queue = &sleeping_queue;
                result.node = n;
                break;
            }
        }
        checked_unlock(&sleeping_queue_lock);
    }

    if(!result.task) {
        checked_lock(&msgwait_queue_lock);
        for(struct list_node* n = msgwait_queue.head; n; n = n->next) {
            struct task* task = n->data;
            if(task->pid == pid) {
                result.task = task;
                result.queue = &msgwait_queue;
                result.node = n;
                break;
            }
        }
        checked_unlock(&msgwait_queue_lock);
    }

    leave_critical_section();

    return result;
}

/* Get port from its number */
static struct port* port_get(unsigned number)
{
    struct port* port = NULL;

    enter_critical_section();
    checked_lock(&port_list_lock);
    for(struct list_node* n = port_list.head; n; n = n->next) {
        if(((struct port*)n->data)->number == number) {
            port = n->data;
            break;
        }
    }
    checked_unlock(&port_list_lock);
    leave_critical_section();
    return port;
}

/* Fork a kernel-mode task */
static unsigned kfork()
{
    volatile unsigned result = 0;
    uint32_t esp = read_esp();

    struct task* new_task = task_create(current_task->name);
    new_task->context.esp = read_esp();
    new_task->context.eflags = read_eflags();
    new_task->context.ebp = read_ebp();
    new_task->context.ebx = read_ebx();         /* maybe not needed because of the barrier() */ 
    new_task->context.esi = read_esi();         /* maybe not needed because of the barrier() */
    new_task->context.edi = read_edi();         /* maybe not needed because of the barrier() */
    new_task->context.cs = KERNEL_CODE_SEG;
    new_task->context.ds = new_task->context.ss = KERNEL_DATA_SEG;
    new_task->context.cr3 = vmm_get_physical(new_task->pagedir);
    new_task->context.eip = (uint32_t)&&after_clone;

    enter_critical_section();
    checked_lock(&ready_queue_lock);
    list_append(&ready_queue, new_task);
    checked_unlock(&ready_queue_lock);
    leave_critical_section();

    result = new_task->pid;

after_clone:
    barrier();  /* re-read all variables from stack */
    return result;
}

static void save_task_state(struct task* task, const struct isr_regs* regs)
{
    /*
     * Save current process context
     * From ring0: useresp is invalid
     */
    unsigned descriptor_mask = ~0x3;
    if((regs->cs & descriptor_mask) == KERNEL_CODE_SEG) {
        task->context.esp = regs->esp + (5 * sizeof(uint32_t));
    } else if((regs->cs & descriptor_mask) == USER_CODE_SEG) {
        task->context.esp = regs->useresp;
    } else {
        panic("Invalid cs value: %p", regs->cs);
    }

    task->context.cs = regs->cs;
    task->context.ds = task->context.ss = regs->ds;
    task->context.eflags = regs->eflags;
    task->context.eip = regs->eip;
    task->context.edi = regs->edi;
    task->context.esi = regs->esi;
    task->context.edx = regs->edx;
    task->context.ecx = regs->ecx;
    task->context.ebx = regs->ebx;
    task->context.eax = regs->eax;
    task->context.ebp = regs->ebp;
}

/* Switch to specified task */
static void task_switch(struct task* task)
{
    assert(task);
    
    cli();
    current_task = task;

    vmm_copy_kernel_mappings(task->pagedir);
    tss_set_kernel_stack(KERNEL_STACK + PAGE_SIZE);
    switch_context(&task->context);

    panic("Invalid code path");
}

/* Switch to next ready task, or idle task */
static void task_switch_next()
{
    struct task* next_task = NULL;

    cli();

    checked_lock(&ready_queue_lock);
    next_task = list_pop(&ready_queue);
    checked_unlock(&ready_queue_lock);

    if(!next_task)
        next_task = idle_task;

    task_switch(next_task);
}

/* Calculate message checksum */
static uint32_t USERFUNC message_checksum(const struct message* msg)
{
    unsigned checksum = hash2(&msg->sender, sizeof(msg->sender), 0);
    checksum = hash2(&msg->reply_port, sizeof(msg->reply_port), checksum);
    checksum = hash2(&msg->code, sizeof(msg->code), checksum);
    checksum = hash2(&msg->len, sizeof(msg->len), checksum);
    checksum = hash2(msg->data, msg->len, checksum);
    return checksum;
}

const char* current_task_name()
{
    return current_task ? current_task->name : NULL;
}

int current_task_pid()
{
    return current_task ? current_task->pid : -1;
}

/****************************************************************************
 * syscall handlers
 ****************************************************************************/
#define SYSCALL_PORTOPEN    0
#define SYSCALL_MSGSEND     1
#define SYSCALL_MSGRECV     2
#define SYSCALL_MSGWAIT     3
#define SYSCALL_MSGPEEK     4
#define SYSCALL_YIELD       5
#define SYSCALL_FORK        6
#define SYSCALL_SETNAME     7

typedef uint32_t (*syscall_handler_t)(struct isr_regs* regs);
static syscall_handler_t syscall_handlers[80] = {0};

/*
 * Open a new port and set receiver to current process
 * Params: None
 * Returns: Port number
 */
static uint32_t syscall_portopen_handler(struct isr_regs* regs)
{
    struct port* result = kmalloc(sizeof(struct port));
    bzero(result, sizeof(struct port));

    enter_critical_section();
    result->number = next_port_value++;
    result->receiver = current_task->pid;
    result->lock = SPINLOCK_INIT;

    checked_lock(&port_list_lock);
    list_append(&port_list, result);
    checked_unlock(&port_list_lock);

    leave_critical_section();

    return result->number;
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
    unsigned port_number = regs->ebx;
    struct message* msg = (struct message*)regs->ecx;

    /* Validate message */
    unsigned checksum = message_checksum(msg);
    assert(checksum == msg->checksum);

    struct port* port = port_get(port_number);
    if(!port)
        return 0;
    
    /* Copy message into kernel space */
    size_t bufsize = sizeof(struct message) + msg->len;
    struct message* msg_copy = kmalloc(bufsize);
    memcpy(msg_copy, msg, bufsize);
    assert(msg_copy->checksum == checksum);

    msg_copy->sender = current_task->pid;
    msg_copy->checksum = message_checksum(msg_copy);

    /* Add message to port's queue */
    checked_lock(&port->lock);
    list_append(&port->queue, msg_copy);
    checked_unlock(&port->lock);

    /* Wake any process waiting on this port */
    checked_lock(&msgwait_queue_lock);
    struct list_node* task_node = msgwait_queue.head;
    while(task_node) {
        struct list_node* next = task_node->next;

        struct task* task = task_node->data;
        if(task->wait_port == port_number) {
            list_remove(&msgwait_queue, task_node);

            checked_lock(&ready_queue_lock);
            list_push(&ready_queue, task);
            checked_unlock(&ready_queue_lock);
        }

        task_node = next;
    }
    checked_unlock(&msgwait_queue_lock);

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

    unsigned port_number = regs->ebx;
    struct message* buffer = (struct message*)regs->ecx;
    uint32_t buffer_size = regs->edx;
    uint32_t* outsize = (uint32_t*)regs->esi;

    struct port* port = NULL;
    while(true) {
        port = port_get(port_number);
        if(!port)
            return 1;

        if(current_task->pid != port->receiver)
            return 2;

        checked_lock(&port->lock);
        bool empty = port->queue.head == NULL;
        checked_unlock(&port->lock);

        if(!empty)
            break;

#if 1
        msgwait(port->number);
#else
        sti();
        hlt();
        cli();
#endif
    }

    uint32_t result;

    checked_lock(&port->lock);
    struct message* message = port->queue.head->data;

    /* Validate message */
    unsigned checksum = message_checksum(message);
    if(checksum != message->checksum) {
        panic("Corrupted message from %d to %d", message->sender, current_task->pid);
    }
    assert(checksum == message->checksum);

    *outsize = sizeof(struct message) + message->len;
    if(buffer_size >= sizeof(struct message) + message->len) {
        memcpy(buffer, message, sizeof(struct message) + message->len);
        list_pop(&port->queue);
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
 */
static uint32_t syscall_msgwait_handler(struct isr_regs* regs)
{
    assert(!interrupts_enabled());
    
    unsigned port_number = regs->ebx;

    save_task_state(current_task, regs);
    current_task->wait_port = port_number;


    checked_lock(&msgwait_queue_lock);
    list_append(&msgwait_queue, current_task);
    checked_unlock(&msgwait_queue_lock);

    /* Switch to next task */
    task_switch_next();
    invalid_code_path();
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
    unsigned port_number = regs->ebx;

    struct port* port = port_get(port_number);
    if(port) {
        checked_lock(&port->lock);
        if(current_task->pid == port->receiver) {
            result = port->queue.head != NULL;
        }
        checked_unlock(&port->lock);
    }
    return result;
}

/*
 * Put current task into tail of ready queue and switch to next ready task
 */
static uint32_t syscall_yield_handler(struct isr_regs* regs)
{
    save_task_state(current_task, regs);

    checked_lock(&ready_queue_lock);
    list_append(&ready_queue, current_task);
    checked_unlock(&ready_queue_lock);

    task_switch_next();
    invalid_code_path();
    return 0;
}

/* 
 * Fork current process
 * Returns:
 *  Parent process:         pid of child process
 *  Child process:          0
 */
static uint32_t syscall_fork_handler(struct isr_regs* regs)
{
    unsigned pid = kfork();
    return pid;
}

static uint32_t syscall_setname_handler(struct isr_regs* regs)
{
    const char* new_name = (const char*)regs->ebx;
    strlcpy(current_task->name, new_name, sizeof(current_task->name));
    return 0;
}

static void syscall_handler(struct isr_regs* regs)
{
    syscall_handler_t handler = NULL;
    unsigned syscall_num = regs->eax;

    if(syscall_num < countof(syscall_handlers))
        handler = syscall_handlers[syscall_num];

    if(handler) {
        regs->eax = handler(regs);
    } else {
        panic("Invalid syscall number: %d", syscall_num);
    }
}

static void syscall_register(unsigned num, syscall_handler_t handler)
{
    if(num < countof(syscall_handlers))
        syscall_handlers[num] = handler;
    else
        panic("Invalid syscall number: %d", num);
}


/****************************************************************************
 * scheduler timer
 ****************************************************************************/
static void scheduler_timer(void* data, const struct isr_regs* regs)
{
    /* Save current task state */
    assert(current_task);
    save_task_state(current_task, regs);

    /* Push current task into ready queue */
    if(current_task != idle_task) {
        checked_lock(&ready_queue_lock);
        list_append(&ready_queue, current_task);
        checked_unlock(&ready_queue_lock);
    }

    task_switch_next();
    invalid_code_path();
}

/****************************************************************************
 * usermode syscall helpers
 ****************************************************************************/

/*
 * Per-task infomation/control block
 */
struct ucb {
    unsigned ack_port;          /* port for receiving acks from kernel */
};
static struct ucb* USERDATA task_ucb = (struct ucb*)0x400000;

static unsigned USERFUNC port_open()
{
    unsigned result = syscall(SYSCALL_PORTOPEN,
                              0,
                              0,
                              0,
                              0,
                              0);
    return result;
}

static bool USERFUNC msgsend(unsigned port, const struct message* msg)
{
    unsigned checksum = message_checksum(msg);
    assert(checksum == msg->checksum);

    unsigned result = syscall(SYSCALL_MSGSEND, 
                              port, 
                              (uint32_t)msg, 
                              0,
                              0,
                              0);
    return result != 0;
}

static unsigned USERFUNC msgrecv(unsigned port, struct message* buffer, size_t buffer_size, size_t* outsize)
{
    /* Buffer validation */
    bzero(buffer, buffer_size);
    unsigned result = syscall(SYSCALL_MSGRECV,
                              port,
                              (uint32_t)buffer,
                              buffer_size,
                              (uint32_t)outsize,
                              0);

    unsigned checksum = message_checksum(buffer);
    assert(buffer->checksum == checksum);

    return result;
}

static void USERFUNC msgwait(unsigned port)
{
    syscall(SYSCALL_MSGWAIT, port, 0, 0, 0, 0);
}

static bool USERFUNC msgpeek(unsigned port)
{
    unsigned ret = syscall(SYSCALL_MSGPEEK,
                           port,
                           0,
                           0,
                           0,
                           0);
    return ret != 0;
}

static void USERFUNC yield()
{
    syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
}

static int USERFUNC user_fork()
{
    int pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);

    if(!pid) {
        task_ucb->ack_port = port_open();
    }

    return pid;
}

#define user_trace(fmt, ...) \
    do {                                                \
        static const char USERRODATA _fmt_[] = fmt;     \
        __user_trace(_fmt_, ##__VA_ARGS__);             \
    } while(0)

static void USERFUNC __user_trace(const char* format, ...)
{
    va_list args;

    unsigned char buffer[128];
    struct message* msg = (struct message*)buffer;

    va_start(args, format);
    vsnprintf((char*)msg->data, sizeof(buffer) - sizeof(struct message), format, args);
    va_end(args);

    msg->sender = 0;
    msg->reply_port = task_ucb->ack_port;
    msg->code = KernelMessageTrace;
    msg->len = strlen((const char*)msg->data) + 1;
    msg->checksum = message_checksum(msg);

    bool ret = msgsend(1, msg);
    assert(ret);

    /* Wait ack */
    size_t outsize;
    unsigned recv_ret = msgrecv(task_ucb->ack_port, msg, sizeof(buffer), &outsize);
    assert(recv_ret == 0);

    assert(msg->checksum == message_checksum(msg));
    assert(msg->sender == 0);
    assert(msg->code == KernelMessageTraceAck);
}

static void USERFUNC user_sleep(unsigned ms)
{
    unsigned char buffer[128];
    struct message* msg = (struct message*)buffer;
    msg->sender = 0;
    msg->reply_port = task_ucb->ack_port;
    msg->code = KernelMessageSleep;
    msg->len = sizeof(uint32_t);
    *((uint32_t*)msg->data) = ms;
    msg->checksum = message_checksum(msg);

    bool ret = msgsend(1, msg);
    assert(ret);

    size_t outsize;
    unsigned recv_ret = msgrecv(task_ucb->ack_port, msg, sizeof(buffer), &outsize);
    assert(recv_ret == 0);
    assert(msg->checksum == message_checksum(msg));
    assert(msg->sender == 0);
    assert(msg->code == KernelMessageSleepAck);
}

static void USERFUNC user_exit()
{
    struct message msg;
    msg.sender = 0;
    msg.reply_port = task_ucb->ack_port;
    msg.code = KernelMessageExit;
    msg.len = 0;
    msg.checksum = message_checksum(&msg);

    bool ret = msgsend(1, &msg);
    assert(ret);

    size_t outsize;
    unsigned recv_ret = msgrecv(task_ucb->ack_port, &msg, sizeof(msg), &outsize);
    invalid_code_path();
}

static void USERFUNC send_ack(unsigned port, unsigned code, uint32_t result)
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

static void USERFUNC setname(const char* new_name)
{
    syscall(SYSCALL_SETNAME, (uint32_t)new_name, 0, 0, 0, 0);
}

/****************************************************************************
 * test usermode programs
 ****************************************************************************/
static unsigned USERFUNC fibonacci(unsigned n)
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

static unsigned USERFUNC is_prime(unsigned num)
{
    if(num == 1)
        return 1;

    for(unsigned i = 2; i < num; i++) {
        if(!(num % i))
            return 0;
    }

    return 1;
}

static void USERFUNC fibonacci_entry()
{
    static const char USERRODATA proc_name[] = "fibonacci";
    setname(proc_name);

    for(unsigned i = 0; i < 37; i++) {
        unsigned fib = fibonacci(i);
        user_trace("fib(%d): %d", i, fib);
    }
}

static void USERFUNC sleeper_entry()
{
    static const char USERRODATA proc_name[] = "sleeper";
    setname(proc_name);

    for(unsigned i = 0; i < 20; i++) {
        user_sleep(1000);
        user_trace("sleeper: %d", i);
    }

    user_exit();
    invalid_code_path();
}

static void USERFUNC user_entry()
{
    static const char USERRODATA proc_name[] = "primes";
    setname(proc_name);

    unsigned start_val = 5001000;

    /* Fork into other tasks */
    unsigned pid = user_fork();
    if(!pid) {
        start_val = 5002000;
    } else {
        pid = user_fork();
        if(!pid) {
            start_val = 5003000;
        } else {
            pid = user_fork();
            if(!pid) {
                fibonacci_entry();
                goto exit;
            } else {
                pid = user_fork();
                if(!pid) {
                    sleeper_entry();
                    goto exit;
                }
            }
        }
    }

    unsigned end_val = start_val + 500;
    for(unsigned i = start_val; i < end_val; i++) {
        if(is_prime(i)) {
            user_trace("%d", i);
        }
    }

exit:
    user_trace("Done");
    user_exit();
    invalid_code_path();
}

/*
 * transform current task into an user task
 */
static void jump_to_usermode(void (*entry_point)())
{
    /* Create ucb */
    vmm_map(task_ucb, pmm_alloc(), VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    bzero(task_ucb, sizeof(struct ucb));

    /* Create port for receiving acks from kernel_task */
    assert(task_ucb);
    task_ucb->ack_port = port_open();

    /* Jump to usermode */
    struct task* task = current_task;

    strlcpy(task->name, "usertask", sizeof(task->name));
    task->context.cs = USER_CODE_SEG | RPL3;
    task->context.ds = task->context.ss = USER_DATA_SEG | RPL3;
    vmm_map(USER_STACK, pmm_alloc(), VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    memset(USER_STACK, 0xCC, PAGE_SIZE);

    task->context.esp = (uint32_t)(USER_STACK + PAGE_SIZE);

    task->context.eflags |= EFLAGS_IF;
    task->context.eip = (uint32_t)entry_point;

    switch_context(&task->context);
    panic("Invalid code path");
}

/****************************************************************************
 * kernel_task and initialization
 ****************************************************************************/
static void idle_task_entry()
{
    while(true) {
        hlt();
    }
}

static void producer_entry()
{
    /* Create ucb */
    vmm_map(task_ucb, pmm_alloc(), VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    bzero(task_ucb, sizeof(struct ucb));

    /* Create port for receiving acks from kernel_task */
    task_ucb->ack_port = port_open();

    size_t bufsize = sizeof(struct message) + 64;
    struct message* msg = kmalloc(bufsize);
    bzero(msg, bufsize);

    unsigned counter = 0;
    while(counter++ < 10) {
        user_trace("counter: %d", counter);
        user_sleep(500);
    }

    user_exit();
    invalid_code_path();
    reboot();
}

static void kernel_task_entry()
{
    /* init port list */
    list_init(&port_list);

    /* Create port for receiving messages from usermode programs */
    unsigned kernel_port = port_open();
    assert(kernel_port == 1);

    /* Create first user task, which will fork into 3 instances */
    unsigned pid = kfork();
    if(!pid) {
        jump_to_usermode(user_entry);
        //producer_entry();
        panic("Invalid code path");
    }

    /* List of sleeping processes and their wake deadlines */
    struct sleeping_task {
        uint64_t deadline;
        int pid;
        unsigned reply_port;
    };
    struct list sleeping_tasks = {0};
    list_init(&sleeping_tasks);

    /* Read messages in a loop */
    size_t bufsiz = sizeof(struct message) + 64;
    struct message* msg = kmalloc(bufsiz);

    while(1) {
        size_t outsiz;
        bzero(msg, bufsiz);

        bool msg_avail = msgpeek(kernel_port);
        if(msg_avail) {
            unsigned recv_ret = msgrecv(kernel_port, msg, bufsiz, &outsiz);
            assert(recv_ret == 0);

            switch(msg->code) {
                case KernelMessageTrace:
                {
                    trace("(%d) %s", msg->sender, msg->data);
                    send_ack(msg->reply_port, KernelMessageTraceAck, 0);
                    break;
                }
                case KernelMessageExit:
                {
                    enter_critical_section();

                    struct task_info task_info = task_getinfo(msg->sender);
                    assert(task_info.task);
                    assert(task_info.node);
                    assert(task_info.queue);

                    trace("Killing task %d", task_info.task->pid);

                    // By the time we get here, the task might have changed queue
                    // Instead, make destroying the task from its current queue atomic
                    // at the syscall level
                    list_remove(task_info.queue, task_info.node);
                    vmm_destroy_pagedir(task_info.task->pagedir);
                    kfree(task_info.task);

                    leave_critical_section();

                    break;
                }
                case KernelMessageSleep:
                {
                    uint32_t* millis = (uint32_t*)msg->data;
                    assert(msg->len >= sizeof(uint32_t));

                    struct sleeping_task* st = kmalloc(sizeof(struct sleeping_task));
                    st->deadline = timer_timestamp() + *millis;
                    st->pid = msg->sender;
                    st->reply_port = msg->reply_port;

                    list_append(&sleeping_tasks, st);
                    break;
                }
                default:
                    panic("Invalid kernel message: %d", msg->code);
                    break;
            }
        }

        /* Wake sleeping processes */
        uint64_t now = timer_timestamp();
        struct list_node* n = sleeping_tasks.head;
        while(n) {
            struct list_node* next = n->next;

            struct sleeping_task* st = n->data;
            if(now >= st->deadline) {
                send_ack(st->reply_port, KernelMessageSleepAck, 0);
                kfree(st);
                list_remove(&sleeping_tasks, n);
            }
            n = next;
        }

        /* If no more processes to run, reboot */
        enter_critical_section();
        if(!ready_queue.head && !sleeping_queue.head && !msgwait_queue.head) {
            trace("No more processes to run. Rebooting");
            reboot();
        }
        leave_critical_section();

        /* Yield */
        yield();
    }
    reboot();
    panic("Invalid code path");
}

static void test_fork()
{
    /* Install scheduler timer */
    timer_schedule(scheduler_timer, NULL, 50, true);

    /* Install syscall handler */
    idt_install(0x80, syscall_handler, true);

    /* Install syscalls */
    syscall_register(SYSCALL_PORTOPEN, syscall_portopen_handler);
    syscall_register(SYSCALL_MSGSEND, syscall_msgsend_handler);
    syscall_register(SYSCALL_MSGRECV, syscall_msgrecv_handler);
    syscall_register(SYSCALL_MSGWAIT, syscall_msgwait_handler);
    syscall_register(SYSCALL_MSGPEEK, syscall_msgpeek_handler);
    syscall_register(SYSCALL_YIELD, syscall_yield_handler);
    syscall_register(SYSCALL_FORK, syscall_fork_handler);
    syscall_register(SYSCALL_SETNAME, syscall_setname_handler);

    /* Map kernel stack */ 
    uint32_t stack_frame = pmm_alloc();
    vmm_map(KERNEL_STACK, stack_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    memset(KERNEL_STACK, 0xCC, PAGE_SIZE);

    tss_set_kernel_stack(KERNEL_STACK + PAGE_SIZE);

    /* Create kernel_task */
    struct task* task = task_create("kernel_task");
    task->context.cs = KERNEL_CODE_SEG;
    task->context.ds = KERNEL_DATA_SEG;
    task->context.ss = KERNEL_DATA_SEG;
    task->context.cr3 = vmm_get_physical(task->pagedir);
    task->context.esp = (uint32_t)(KERNEL_STACK + PAGE_SIZE);
    task->context.eflags = read_eflags() | EFLAGS_IF;
    task->context.eip = (uint32_t)kernel_task_entry;
    kernel_task = task;

    /* Create idle_task */
    struct task* task1 = task_create("idle_task");
    task1->context.cs = KERNEL_CODE_SEG;
    task1->context.ds = task1->context.ss = KERNEL_DATA_SEG;
    task1->context.cr3 = vmm_get_physical(task1->pagedir);
    task1->context.esp = (uint32_t)(KERNEL_STACK + PAGE_SIZE);
    task1->context.eflags = read_eflags() | EFLAGS_IF;
    task1->context.eip = (uint32_t)idle_task_entry;
    idle_task = task1;

    /* Switch to kernel task */
    task_switch(task);
    invalid_code_path();
}

void test_scheduler()
{
    trace("Testing scheduler");

    heap_record_start();
    test_fork();
    heap_record_stop();
}



