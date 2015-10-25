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

    struct Message_t msg;
    while(1) {
        ret = rs_port_read(RS_PORT_VGA, &msg);
        if(!ret) {
            rs_trace("rs_port_read() failed");
            return 1;
        }

        switch(msg.id) {
            case RS_MSG_VGA_WRITE_STRING:
                msg.payload[sizeof(msg.payload) - 1] = '\0';
                write_string_attr((char*)msg.payload, COLOR_WHITE);
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



