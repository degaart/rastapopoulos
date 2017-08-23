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
 *
 *
 * Task states
 *  - Ready to be run
 *  - Sleeping until condition is met:
 *      - Deadline
 *      - Wait for port to accept messages
 *      - Wait for message to be available on port
 *  - Exited
 *
 * We only keep these 3 queues to facilitate implementing things
 * like waiting for a message with a timeout
 *
 * PIDs are signed ints >= 0
 * 
 */

#define USER_STACK          ((unsigned char*)0xBFFFC000)
#define KERNEL_STACK        ((unsigned char*)0xBFFFE000)

#define TASK_NAME_MAX       32
#define INVALID_PID         (-1)
#define SLEEP_INFINITE      0xFFFFFFFF

/*
 * Save current task state
 * Should only be called from syscall_handler()
 */
void save_current_task_state(const struct isr_regs* regs);

/*
 * Put current task into sleeping queue
 */
void task_block(int canrecv_port, int cansend_port, unsigned timeout);

/*
 * Remove task from sleeping queue and put into ready queue
 */
void task_wake(int pid);

/*
 * Wake tasks waiting for port to be able to receive message
 */
void wake_tasks_waiting_for_port(int port_number);

const char* current_task_name();

void current_task_set_name(const char* name);

int current_task_pid();

void jump_to_usermode(void (*user_entry)());

void scheduler_start();



