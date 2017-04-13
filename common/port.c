#include "port.h"
#include "syscall.h"
#include "util.h"
#include "debug.h"
#include "string.h"

/* Calculate message checksum */
uint32_t message_checksum(const struct message* msg)
{
    unsigned checksum = hash2(&msg->sender, sizeof(msg->sender), 0);
    checksum = hash2(&msg->reply_port, sizeof(msg->reply_port), checksum);
    checksum = hash2(&msg->code, sizeof(msg->code), checksum);
    checksum = hash2(&msg->len, sizeof(msg->len), checksum);
    checksum = hash2(msg->data, msg->len, checksum);
    return checksum;
}

void msgwait(int port)
{
    syscall(SYSCALL_MSGWAIT, port, 0, 0, 0, 0);
}

int port_open(int port_number)
{
    int result = syscall(SYSCALL_PORTOPEN,
                         port_number,
                         0,
                         0,
                         0,
                         0);
    return result;
}

bool msgsend(int port, const struct message* msg)
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

unsigned msgrecv(int port, struct message* buffer, unsigned buffer_size, unsigned* outsize)
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

bool msgpeek(int port)
{
    unsigned ret = syscall(SYSCALL_MSGPEEK,
                           port,
                           0,
                           0,
                           0,
                           0);
    return ret != 0;
}

