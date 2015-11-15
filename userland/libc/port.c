#include <rasta.h>
#include <syscall.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

uint32_t rs_port_open(uint32_t port_number) {
    return rs_syscall(SYSCALL_PORT_OPEN, port_number, 0, 0);
}

uint32_t rs_port_close(uint32_t port_number) {
    return rs_syscall(SYSCALL_PORT_CLOSE, port_number, 0, 0);
}

uint32_t rs_port_send(uint32_t port_number, const struct Message_t* msg) {
    /* Usermode check that all params are valid */
    // assert(msg->payload_size < INT32_MAX);
    // for(uint8_t* p = (uint8_t*) msg->payload; p < ((uint8_t*)msg->payload) + msg->payload_size; p++ ) {
    //     volatile uint8_t* ch = p;
    //     *ch = (*ch) + 0;
    // }
    return rs_syscall(SYSCALL_PORT_SEND, port_number, (uint32_t)msg, 0);
}

uint32_t rs_port_read(uint32_t port_number, struct Message_t* buffer) {
    /* Usermode check that all params are valid */
    // assert(buffer->payload_size < INT32_MAX);
    // bzero(buffer->payload, buffer->payload_size);

    return rs_syscall(SYSCALL_PORT_READ, port_number, (uint32_t)buffer, 0);
}
