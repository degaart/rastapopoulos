#include "runtime.h"
#include "io.h"
#include "port.h"
#include "debug.h"
#include "logger.h"

static void debug_out(const char* message)
{
    while(*message) {
        outb(0xE9, *message);
        message++;
    }
}

void main()
{
    setname("logger");

    int ret = port_open(LoggerPort);
    if(ret < 0) {
        panic("Failed to open logger port");
    }

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

        if(msg->code == LoggerMessageTrace) {
            debug_out("▶ ");
            debug_out(msg->data);
            debug_out("\n");
            send_ack(msg->reply_port, LoggerMessageTraceAck, 0);
        } else {
            panic("Invalid message code");
        }
    }
}


