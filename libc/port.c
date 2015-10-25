#include <rasta.h>
#include <syscall.h>

uint32_t rs_port_open(uint32_t port_number) {
    return rs_syscall(SYSCALL_PORT_OPEN, port_number, 0, 0);
}

uint32_t rs_port_close(uint32_t port_number) {
    return rs_syscall(SYSCALL_PORT_CLOSE, port_number, 0, 0);
}

uint32_t rs_port_send(uint32_t port_number, const struct Message_t* msg) {
    return rs_syscall(SYSCALL_PORT_SEND, port_number, (uint32_t)msg, 0);
}

uint32_t rs_port_read(uint32_t port_number, struct Message_t* buffer) {
    return rs_syscall(SYSCALL_PORT_READ, port_number, (uint32_t)buffer, 0);
}
