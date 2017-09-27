#include <stddef.h>
#include <port.h>
#include <debug.h>
#include "runtime.h"
#include "vfs_client.h"

int close(int fd)
{
    if(fd == -1)
        return -1;

    int ret;
    int rpc_ret = vfs_close(&ret,
                            VFSPort,
                            pcb.ack_port,
                            fd);
    handle_rpc_ret(rpc_ret);
    return ret;
}



