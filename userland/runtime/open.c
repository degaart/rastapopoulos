#include <stddef.h>
#include <port.h>
#include <debug.h>
#include "runtime.h"
#include "vfs_client.h"

int open(const char* filename, unsigned mode, int perm)
{
    assert(filename != NULL);
    assert(mode == O_RDONLY);

    int ret;
    int rpc_ret = vfs_open(&ret,
                           VFSPort,
                           pcb.ack_port,
                           filename,
                           mode,
                           perm);
    handle_rpc_ret(rpc_ret);
    return ret;
}

