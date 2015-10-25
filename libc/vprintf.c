#include <stdio.h>
#include <string.h>
#include <rasta.h>

struct buffer_t {
    char buffer[81];
    char guard;
    char* ptr;
    int result;
};

static void emit_string(const char* str, unsigned size) {
    struct Message_t msg;
    bzero(&msg, sizeof(msg));
    msg.id = RS_MSG_VGA_WRITE_STRING;
    msg.payload = (void*)str;
    msg.payload_size = size;

    while(!rs_port_send(RS_PORT_VGA, &msg)) {
        rs_trace("Failed to send message to RS_PORT_VGA. Retrying...");
        rs_yield();
    }
}

static void vprintf_callback(int ch, void* args) {
    struct buffer_t* buf = (struct buffer_t*)args;
    if(buf->ptr >= buf->buffer + sizeof(buf->buffer) - 1) {
        emit_string(buf->buffer, buf->ptr - buf->buffer);
        buf->ptr = buf->buffer;
    }

    buf->result++;
    *buf->ptr = ch;
    buf->ptr++;
}

int vprintf(const char * restrict format, va_list ap) {
    struct buffer_t buffer;
    bzero(&buffer, sizeof(struct buffer_t));
    buffer.ptr = buffer.buffer;

    formatv(vprintf_callback, &buffer, format, ap);

    if(buffer.result && buffer.ptr != buffer.buffer) {
        emit_string(buffer.buffer, buffer.ptr - buffer.buffer);
    }
    return buffer.result;
}

