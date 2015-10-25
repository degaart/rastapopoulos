#include <rasta.h>
#include <string.h>

int main() {
    rs_trace("Hello started");

    struct Message_t msg;
    bzero(&msg, sizeof(msg));

    msg.id = RS_MSG_VGA_WRITE_STRING;
    msg.payload = "HELLO";
    msg.payload_size = 6;

    uint32_t ret = rs_port_send(RS_PORT_VGA, &msg);
    if(!ret)
        rs_trace("Failed to send message");

    rs_trace("Hello exited");
    return 0;
}


