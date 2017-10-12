#include <port.h>
#include <debug.h>
#include <string.h>
#include "runtime.h"
#include "kernel_task_client.h"

int int10(struct int10_regs* regs)
{
    struct int10_regs buffer;
    size_t buffer_size = sizeof(buffer);

    int ret;
    int rpc_ret = kernel_int10(&ret,
                               KernelPort,
                               pcb.ack_port,
                               &buffer,
                               &buffer_size,
                               regs,
                               sizeof(struct int10_regs));
    handle_rpc_ret(rpc_ret);
    memcpy(regs, &buffer, buffer_size);
    return ret;
}

