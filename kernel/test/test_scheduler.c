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
};

struct message {
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
    struct list message_queue;

    uint64_t sleep_deadline;
};

static struct list ready_queue = {0};
static struct list exited_queue = {0};
static struct list sleeping_queue = {0};
static struct list port_list = {0};

static struct task* kernel_task = NULL;
static struct task* current_task = NULL;

static int next_pid_value = 0;
static unsigned next_port_value = 1; 

extern uint32_t syscall(uint32_t eax, uint32_t ebx,
                        uint32_t ecx, uint32_t edx,
                        uint32_t esi, uint32_t edi);

#define KernelMessageTrace      3
#define KernelMessageTraceAck   4

#if 0
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
#endif

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
    list_init(&result->message_queue);

    return result;
}

/*
 * Get a task by it's pid
 */
static struct task* task_get(int pid)
{
    static struct task* result = NULL;

    enter_critical_section();
    for(struct list_node* n = ready_queue.head; n; n = n->next) {
        struct task* task = n->data;
        if(task->pid == pid) {
            result = task;
            break;
        }
    }

    if(!result) {
        for(struct list_node* n = sleeping_queue.head; n; n = n->next) {
            struct task* task = n->data;
            if(task->pid == pid) {
                result = task;
                break;
            }
        }
    }

    leave_critical_section();

    return result;
}

/* Get port from its number */
static struct port* port_get(unsigned number)
{
    struct port* port = NULL;

    enter_critical_section();
    for(struct list_node* n = port_list.head; n; n = n->next) {
        if(((struct port*)n->data)->number == number) {
            port = n->data;
            break;
        }
    }
    leave_critical_section();
    return port;
}

/* Fork a kernel-mode task */
static unsigned kfork()
{
    volatile unsigned result = 0;
    uint32_t esp = read_esp();
    dump_var(read_cr3());
    dump_var(esp);

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
    list_append(&ready_queue, new_task);
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
    current_task = task;

    vmm_copy_kernel_mappings(task->pagedir);
    tss_set_kernel_stack(KERNEL_STACK + PAGE_SIZE);
    switch_context(&task->context);

    panic("Invalid code path");
}

#define SYSCALL_PORTOPEN    0
#define SYSCALL_MSGSEND     1
#define SYSCALL_MSGRECV     2

typedef uint32_t (*syscall_handler_t)(struct isr_regs* regs);
static syscall_handler_t syscall_handlers[80] = {0};

#if 0
static uint32_t syscall_exit_handler(struct isr_regs* regs)
{
    /*
     * TODO: Remove this syscall and instead send message
     * to kernel_task
     */
    struct task* task = current_task;

    /* 
     * Cannot destroy task inside itself 'cause then the pagedir would be invalidated
     * Instead, place it into a task recycle bin and let kernel_task handle its destruction
     */
    assert(task != kernel_task);

    /* Add to exited tasks */
    list_push(&exited_queue, task);

    /* Pop next task from ready queue */
    struct task* next_task = list_pop(&ready_queue);
    task_switch(next_task);

    panic("Invalid code path");
    return 0;
}

static uint32_t syscall_fork_handler(struct isr_regs* regs)
{
    unsigned pid = kfork();
    return pid;
}

static uint32_t syscall_sleep_handler(struct isr_regs* regs)
{
    unsigned millis = regs->ebx;

    /* Must save task state */
    save_task_state(current_task, regs);

    /* Determine deadline */
    uint64_t now = timer_timestamp();
    current_task->sleep_deadline = now + millis;
    
    /* Push task into sleeping queue */
    list_push(&sleeping_queue, current_task);

    /* Switch to  next task */
    struct task* next_task = list_pop(&ready_queue);
    assert(next_task);
    task_switch(next_task);

    invalid_code_path();
    return 0;
}
#endif

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
    list_append(&port_list, result);
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

    struct port* port = port_get(port_number);
    if(!port)
        return 0;
    
    /* Copy message into kernel space */
    struct message* msg_copy = kmalloc(sizeof(struct message) + msg->len);
    memcpy(msg_copy, msg, sizeof(struct message) + msg->len);
    msg_copy->sender = current_task->pid;

    /* Add message to port's queue */
    list_append(&port->queue, msg_copy);

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
    unsigned port_number = regs->ebx;
    struct message* buffer = (struct message*)regs->ecx;
    uint32_t buffer_size = regs->edx;
    uint32_t* outsize = (uint32_t*)regs->esi;

    struct port* port = port_get(port_number);
    if(!port)
        return 1;

    if(current_task->pid != port->receiver)
        return 2;

    while(!port->queue.head) {
        sti();
        hlt();          // TODO: Sleep task and switch to another task
        cli();
    }

    struct message* message = port->queue.head->data;

    *outsize = sizeof(struct message) + message->len;
    if(buffer_size >= sizeof(struct message) + message->len) {
        memcpy(buffer, message, sizeof(struct message) + message->len);
        list_pop(&port->queue);
        return 0;
    } else {
        return 3;
    }

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

static void scheduler_timer(void* data, const struct isr_regs* regs)
{
    /* Save current task state */
    assert(current_task);
    save_task_state(current_task, regs);

    /* Push current task into ready queue */
    list_append(&ready_queue, current_task);

    /* Handle sleeping tasks */
    uint64_t ts = timer_timestamp();
    struct list_node* awake_task = NULL;
    for(struct list_node* t = sleeping_queue.head; t; t = t->next) {
        struct task* task = t->data;
        if(ts >= task->sleep_deadline) {
            /* Switch to this */
            awake_task = t;
            break;
        }
    }

    /* Determine next task to run */
    struct task* next_task = NULL;
    if(awake_task) {
        list_remove(&sleeping_queue, awake_task);
        next_task = awake_task->data;
    } else {
        next_task = list_pop(&ready_queue);
    }

    /* Switch to next task */
    task_switch(next_task);
    invalid_code_path();
}

static unsigned port_open()
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
    unsigned result = syscall(SYSCALL_MSGRECV,
                              port,
                              (uint32_t)buffer,
                              buffer_size,
                              (uint32_t)outsize,
                              0);
    return result;
}

#if 0
static void USERFUNC user_trace(const char* text)
{
    msgsend(text,
            strlen(text) + 1,
            0,
            KernelMessageTrace);
}

static void USERFUNC fibonacci_entry()
{
    for(unsigned i = 0; i < 37; i++) {
        unsigned fib = fibonacci(i);

        static char USERDATA msg[64];
        static const char USERRODATA fmt[] = "fib(%d): %d";
        snprintf(msg, sizeof(msg), fmt, i, fib);

        user_trace(msg);
    }
}

static void USERFUNC sleeper_entry()
{
    for(unsigned i = 0; i < 29; i++) {
        syscall(SYSCALL_SLEEP, 1000, 0, 0, 0, 0);

        static char USERDATA msg[64];
        static const char USERRODATA fmt[] = "sleeper: %d";
        snprintf(msg, sizeof(msg), fmt, i);

        user_trace(msg);
    }

    syscall(SYSCALL_EXIT, 0, 0, 0, 0, 0);
    invalid_code_path();
}

static void USERFUNC user_entry()
{
    static char USERDATA done_msg[] = "Done";

    unsigned start_val = 5001000;

    /* Fork into two other tasks */
    unsigned pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
    if(!pid) {
        start_val = 5002000;
    } else {
        pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
        if(!pid) {
            start_val = 5003000;
        } else {
            pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
            if(!pid) {
                fibonacci_entry();
                goto exit;
            } else {
                pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
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
            static char USERDATA msg[64];
            static const char USERRODATA fmt[] = "%d";
            snprintf(msg, sizeof(msg), fmt, i);
            user_trace(msg);
        }
    }

exit:
    user_trace(done_msg);

    syscall(SYSCALL_EXIT, 0, 0, 0, 0, 0);
    invalid_code_path();             /* Should give a page fault */
    while(1);
}

/*
 * transform current task into an user task
 */
static void jump_to_usermode(void (*entry_point)())
{
    struct task* task = current_task;

    strlcpy(task->name, "usertask", sizeof(task->name));
    task->context.cs = USER_CODE_SEG | RPL3;
    task->context.ds = task->context.ss = USER_DATA_SEG | RPL3;
    vmm_map(USER_STACK, pmm_alloc(), VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    memset(USER_STACK, 0xCC, PAGE_SIZE);

    task->context.esp = (uint32_t)(USER_STACK + PAGE_SIZE);

    task->context.eflags |= EFLAGS_IF;
    task->context.eip = (uint32_t)entry_point;
    task->context.eax = kernel_port->number;

    switch_context(&task->context);
    panic("Invalid code path");
}
#endif

static void producer_entry()
{
    /* Create port for receiving acks from kernel_task */
    unsigned ack_port = port_open();

    size_t bufsize = sizeof(struct message) + 64;
    struct message* msg = kmalloc(bufsize);
    bzero(msg, bufsize);

    unsigned counter = 0;
    while(counter++ < 10) {
        char text[64];
        snprintf(text, sizeof(text), "counter: %d", counter);

        msg->reply_port = ack_port;
        msg->code = KernelMessageTrace;
        msg->len = strlen(text) + 1;
        memcpy(msg->data, text, msg->len);

        /* Send message */
        bool send_ret = msgsend(1, msg);
        assert(send_ret != false);

        /* Wait ack */
        size_t outsize;
        unsigned recv_ret = msgrecv(ack_port, msg, bufsize, &outsize);
        assert(recv_ret == 0);
    }
    reboot();
}

static void kernel_task_entry()
{
    /* init port list */
    list_init(&port_list);

    /* Create port for receiving messages from usermode programs */
    unsigned kernel_port = port_open();
    assert(kernel_port == 1);

    /* initialization: create first user task, which will fork into 3 instances */
    unsigned pid = kfork();
    if(!pid) {
        //jump_to_usermode(user_entry);
        producer_entry();
        panic("Invalid code path");
    }

    /* Sit idle until we get a task to reap */
    size_t bufsiz = sizeof(struct message) + 64;
    struct message* msg = kmalloc(bufsiz);

    while(1) {
        size_t outsiz;
        bzero(msg, bufsiz);

        unsigned recv_ret = msgrecv(kernel_port, msg, bufsiz, &outsiz);
        assert(recv_ret == 0);

        switch(msg->code) {
            case KernelMessageTrace:
            {
                trace("(%d) %s", msg->sender, msg->data);

                struct message ack = {0};
                ack.code = KernelMessageTraceAck;
                ack.len = 0;
                bool send_ret = msgsend(msg->reply_port, &ack);
                assert(send_ret);
                break;
            }
            default:
                panic("Invalid kernel message: %d", msg->code);
                break;
        }
#if 0
        bool has_messages = msgpeek(kernel_port);
        if(has_messages) {
            uint32_t result = msgrecv(kernel_port, message, len);
            if(result) {
                len = result;
                kfree(message);
                message = kmalloc(len);
            } else {
                switch(message->id) {
                    case KernelMessageTrace:
                        trace("(%d) %s", message->sender, message->data);
                        break;
                    case KernelMessageExit:

                        break;
                    default:
                        panic("Invalid kernel message id: %d", message->id);
                        break;
                }
            }
        } else {
            enter_critical_section();

            if(exited_queue.head) {
                struct task* task;
                while((task = list_pop(&exited_queue))) {
                    vmm_destroy_pagedir(task->pagedir);
                    kfree(task);
                }
            }
            if(!ready_queue.head)
                break;

            leave_critical_section();

            hlt();
        }
#endif
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

    /* Switch to kernel task */
    task_switch(task);
    invalid_code_path();
}

void test_scheduler()
{
    trace("Testing scheduler");

    test_fork();
}



