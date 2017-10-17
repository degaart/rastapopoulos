#include <runtime.h>
#include <port.h>
#include <debug.h>
#include "vga_client.h"

void puts(const char* str)
{
    int rpc_ret = vga_write_string(VGAPort, pcb.ack_port, str);
    handle_rpc_ret(rpc_ret);
}

