#include "syscall_handler.h"
#include "util.h"
#include "kmalloc.h"
#include "string.h"
#include "idt.h"
#include "scheduler.h"
#include "kdebug.h"

static syscall_handler_t syscall_handlers[80] = {0};

static void syscall_handler(struct isr_regs* regs)
{
    syscall_handler_t handler = NULL;
    unsigned syscall_num = regs->eax;

    if(syscall_num < countof(syscall_handlers))
        handler = syscall_handlers[syscall_num];

    if(handler) {
#ifdef SYSCALL_TRACE
        trace("syscall: %d, pid: %d, name: %s", 
              syscall_num, 
              current_task->pid, 
              current_task->name);
#endif

        save_current_task_state(regs);
        kernel_heap_check();
        regs->eax = handler(regs);
        kernel_heap_check();
    } else {
        panic("Invalid syscall number: %d", syscall_num);
    }
}

void syscall_register(unsigned num, syscall_handler_t handler)
{
    if(num < countof(syscall_handlers))
        syscall_handlers[num] = handler;
    else
        panic("Invalid syscall number: %d", num);
}

void syscall_init()
{
    bzero(syscall_handlers, sizeof(syscall_handlers));

    /* Install syscall handler */
    idt_install(0x80, syscall_handler, true);
}

