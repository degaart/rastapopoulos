#include <rasta.h>
#include <syscall.h>
#include <string.h>

struct buffer_t {
    char buf[512];
    int idx;
};

static void output_char(int ch, void* args) {
    struct buffer_t* buffer = (struct buffer_t*)args;
    
    if(buffer->idx < sizeof(buffer->buf) - 2) {
        buffer->buf[buffer->idx] = ch;
        buffer->idx++;    
    }
}

void rs_trace(const char* str, ...) {
    struct buffer_t buffer;
    buffer.idx = 0;

    va_list args;
    va_start(args, str);
    formatv(output_char, &buffer, str, args);
    va_end(args);

    buffer.buf[buffer.idx] = '\0';
    rs_syscall(SYSCALL_TRACE, (uint32_t)buffer.buf, 0, 0);
}


