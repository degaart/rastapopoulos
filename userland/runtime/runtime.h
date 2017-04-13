#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "task_info.h"

/* Program control block */
struct pcb {
    int ack_port;
};

void yield();
int fork();
void sleep(unsigned ms);
void exit();
void setname(const char* new_name);
void reboot();
void send_ack(int port, unsigned code, uint32_t result);
void exec(const char* filename);

struct task_info;
bool get_task_info(int pid, struct task_info* buffer);

