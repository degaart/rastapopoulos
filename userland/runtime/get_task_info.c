#include <stddef.h>
#include <port.h>
#include <debug.h>
#include "runtime.h"
#include "kernel_task_client.h"

bool get_task_info(int pid, struct task_info* buffer)
{
    size_t buffer_size = sizeof(*buffer);
    int ret;
    int rpc_ret = kernel_get_task_info(&ret,
                                       KernelPort,
                                       pcb.ack_port,
                                       pid,
                                       buffer,
                                       &buffer_size);
    if(rpc_ret != RPC_OK) /* This is used by the logger, do not panic if it doesnt work */
        return false;
    else if(ret)
        return false;
    else if(buffer_size < sizeof(*buffer))
        return false;
    return true;
}

