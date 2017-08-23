#include "runtime.h"
#include "io.h"
#include "port.h"
#include "debug.h"
#include "logger.h"
#include "string.h"

void main()
{
    int ret = port_open(LoggerPort);
    if(ret < 0) {
        panic("Failed to open logger port");
    }

    char msg_buf[512];
    unsigned char buffer[512];
    struct message* msg = (struct message*)buffer;
    while(1) {
        unsigned outsiz;
        int ret = msgrecv(LoggerPort, 
                          msg, 
                          sizeof(buffer), 
                          &outsiz);
        if(ret != 0) {
            panic("msgrecv failed");
        }

        struct task_info sender_info;
        bool got_info = get_task_info(msg->sender, &sender_info);
        if(!got_info) {
            sender_info.pid = msg->sender;
            strlcpy(sender_info.name, "", sizeof(sender_info.name));
        }

        if(msg->code == LoggerMessageTrace) {
            snprintf(msg_buf, sizeof(msg_buf),
                     "▶ [%s/%d] ",
                     sender_info.name, sender_info.pid);
            debug_write(msg_buf);
            debug_write(msg->data);
            debug_write("\n");
        } else {
            snprintf(msg_buf, sizeof(msg_buf), 
                     "▶ [%s/%d] Invalid message code %d\n",
                     sender_info.name, sender_info.pid,
                     msg->code);
            debug_write(msg_buf);
        }
    }
}


