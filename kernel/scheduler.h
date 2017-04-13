#pragma once

#include "vmm.h"
#include "list.h"
#include "context.h"
#include "idt.h"

#include <stdint.h>


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

struct task {
    list_declare_node(task) node;
    int pid;
    char name[32];
    struct pagedir* pagedir;
    struct context context;

    unsigned wait_port;
    uint64_t sleep_deadline;
};
list_declare(task_list, task);

void scheduler_start(void (*user_entry)());
int current_task_pid();
const char* current_task_name();
void current_task_set_name(const char* name);
void task_wait_message(int port_number, const struct isr_regs* regs);
void wake_tasks_for_port(int port_number);
void save_current_task_state(const struct isr_regs* regs);
void jump_to_usermode();
struct task* task_get(int pid);



