#include "runtime.h"
#include "io.h"
#include "port.h"
#include "debug.h"
#include "logger.h"
#include "string.h"
#include "logger_common.h"
#include "logger_server.h"

void handle_logger_trace(int sender_pid, const char* msg)
{
    debug_write("       ");

    struct task_info sender_info;
    if(get_task_info(sender_pid, &sender_info)) {
        char prefix[96];
        snprintf(prefix, sizeof(prefix),
                 "[%s (%d)] ",
                 sender_info.name,
                 sender_info.pid);
        debug_write(prefix);
    }

    debug_write(msg);
    debug_write("\n");
}

void main()
{
    int ret = port_open(LoggerPort);
    if(ret < 0) {
        panic("Failed to open logger port");
    }

    rpc_dispatch(LoggerPort);
}


