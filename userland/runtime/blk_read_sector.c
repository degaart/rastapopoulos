#include <stddef.h>
#include <runtime.h>
#include <debug.h>
#include <limits.h>
#include "blk_common.h"
#include "blk_client.h"

int blk_read_sector(void* buffer, size_t size, uint32_t sector)
{
    assert(size >= 512);

    int ret;
    int rpc_ret = blkimpl_read_sector(&ret,
                                      BlkPort,
                                      pcb.ack_port,
                                      buffer,
                                      &size,
                                      sector);

    handle_rpc_ret(ret);
    return ret;
}


