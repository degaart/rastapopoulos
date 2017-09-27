#include "logger_client.h"
#include "runtime.h"
#include <stdarg.h>
#include <string.h>
#include <port.h>

void __log(const char* func, const char* file, int line, const char* fmt, ...)
{
    // Buffers
    char buffer[128];
    char* ptr = buffer;
    size_t ptr_size = sizeof(buffer);
    
    // strip path from file
    const char* basename = file + strlen(file);
    while(basename >= file && *basename != '/')
        basename--;
    if(*basename == '/')
        basename++;
    else if(!*basename)
        basename = file;

    snprintf(ptr, ptr_size,
             "[%s:%d] ", basename, line);
    size_t len = strlen(ptr);
    ptr += len;
    ptr_size -= len;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ptr, ptr_size, fmt, args);
    va_end(args);

    int rpc_ret = logger_trace(LoggerPort, pcb.ack_port, buffer);
    if(rpc_ret != RPC_OK) {
        debug_write("[emergency log] ");
        debug_write(buffer);
        debug_write("\n");
    }
}




