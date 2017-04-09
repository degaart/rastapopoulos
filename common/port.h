#pragma once

#include "list.h"
#include "spinlock.h"

#include <stdint.h>
#include <stdbool.h>

/*
 * Port: one-way communication channel between processes
 * Blocking receive and sends
 * Identified by a single, unique number
 * Single receiver, multiple senders
 */
list_declare(message_list, message);

struct port {
    list_declare_node(port) node;
    int number;            /* Port number */
    int receiver;
    struct message_list queue; /* message queue */
    spinlock_t lock;
};
list_declare(port_list, port);

struct message {
    list_declare_node(message) node;
    uint32_t checksum;
    int sender;                 /* Sending process pid */
    int reply_port;             /* Port number to send response to */
    unsigned code;              /* Message code, interpretation depends on receiver */
    unsigned len;                 /* Length of data[] */
    unsigned char data[];
};

void msgwait(int port);
uint32_t message_checksum(const struct message* msg);
int port_open(int port_number);
bool msgsend(int port, const struct message* msg);
unsigned msgrecv(int port, struct message* buffer, unsigned buffer_size, unsigned* outsize);
bool msgpeek(int port);

