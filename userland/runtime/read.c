#include <stddef.h>
#include <port.h>
#include <debug.h>
#include "runtime.h"
#include "vfs_client.h"

int read(int fd, void* buffer, size_t size)
{
    if(size == 0)
        return 0;
    if(fd == -1)
        return -1;
    if(buffer == NULL)
        return -1;

    size_t buffer_size = size;
    int ret;
    int rpc_ret = vfs_read(&ret,
                           VFSPort,
                           pcb.ack_port,
                           buffer, &buffer_size,
                           fd,
                           size);
    handle_rpc_ret(rpc_ret);
    return ret;
}


