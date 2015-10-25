#include <rasta.h>
#include "vga.h"

int main() {
    rs_trace("vgadrv started");
    rs_mmap((void*)VGA_BASE, VGA_BASE, MMAP_WRITABLE);

    vga_init();

    uint32_t ret = rs_port_open(RS_PORT_VGA);
    if(ret == 0) {
        rs_trace("Failed to open port %u", RS_PORT_VGA);
        return 1;
    }

    char buffer[512];
    struct Message_t msg;
    while(1) {
        msg.payload = buffer;
        msg.payload_size = sizeof(buffer);

        ret = rs_port_read(RS_PORT_VGA, &msg);
        if(ret) {
            rs_trace("rs_port_read() failed: need %u bytes, have %u bytes", ret, sizeof(buffer));
            return 1;
        }

        switch(msg.id) {
            case RS_MSG_VGA_WRITE_STRING:
                buffer[msg.payload_size - 1] = '\0';
                write_string_attr(buffer, COLOR_WHITE);
                write_string_attr("\n", COLOR_WHITE);
                break;
            default:
                rs_trace("Unhandled message id: 0x%X", msg.id);
                break;
        }
    }
    rs_port_close(RS_PORT_VGA);
    return 0;
}



