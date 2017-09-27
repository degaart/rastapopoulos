#include <stddef.h>
#include <runtime.h>
#include <debug.h>
#include <limits.h>
#include "blockdrv_common.h"
#include "blockdrv_client.h"


int blk_read(void* buffer, size_t size, uint64_t offset)
{
    assert(offset < INT_MAX);

    int ret;
    int rpc_ret = blkimpl_read(&ret,
                               BlkPort,
                               pcb.ack_port,
                               buffer,
                               &size,
                               (int)offset);
    handle_rpc_ret(rpc_ret);
    return ret;
}


