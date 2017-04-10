#pragma once

#include <stdint.h>

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

