#include <stddef.h>
#include <runtime.h>
#include <debug.h>
#include <limits.h>
#include "blockdrv_common.h"
#include "blockdrv_client.h"

uint64_t blk_total_size()
{
    long long ret;
    int rpc_ret = blkimpl_total_size(&ret,
                                     BlkPort,
                                     pcb.ack_port);
    handle_rpc_ret(rpc_ret);
    return ret;
}

