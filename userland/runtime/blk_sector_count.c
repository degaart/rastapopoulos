#include <stddef.h>
#include <runtime.h>
#include <debug.h>
#include <limits.h>
#include "blockdrv_common.h"
#include "blockdrv_client.h"

uint32_t blk_sector_count()
{
    int ret;
    int rpc_ret = blkimpl_sector_count(&ret,
                                       BlkPort,
                                       pcb.ack_port);
    handle_rpc_ret(rpc_ret);
    return ret;
}


