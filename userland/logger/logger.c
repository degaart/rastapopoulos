#include "runtime.h"
#include "io.h"
#include "port.h"
#include "debug.h"
#include "logger.h"
#include "string.h"

static void debug_out(const char* message)
{
    while(*message) {
        outb(0xE9, *message);
        message++;
    }
}

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
        unsigned ret = msgrecv(LoggerPort, 
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
            debug_out(msg_buf);
            debug_out(msg->data);
            debug_out("\n");

            send_ack(msg->reply_port, LoggerMessageTraceAck, 1);
        } else {
            snprintf(msg_buf, sizeof(msg_buf), 
                     "▶ [%s/%d] Invalid message code %d\n",
                     sender_info.name, sender_info.pid,
                     msg->code);
            debug_out(msg_buf);
            send_ack(msg->reply_port, LoggerMessageTraceAck, 0);
        }
    }
}


