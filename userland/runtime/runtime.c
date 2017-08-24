#include "runtime.h"
#include "syscall.h"
#include "io.h"
#include "string.h"
#include "port.h"
#include "debug.h"
#include "../logger/logger.h"
#include "task_info.h"
#include "../vfs/vfs.h"
#include "../../kernel/kernel_task.h"
#include "serializer.h"
#include <stdarg.h>

struct pcb pcb;

void yield()
{
    syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
}

int fork()
{
    int pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
    if(pid == 0) {
        pcb.ack_port = port_open(-1);
    }
    return pid;
}

void sleep(unsigned ms)
{
    syscall(SYSCALL_SLEEP,
            ms,
            0,
            0,
            0,
            0);
}

void exit()
{
    syscall(SYSCALL_EXIT,
            0,
            0,
            0,
            0,
            0);
}

void abort()
{
    reboot();
}

void reboot()
{
    syscall(SYSCALL_REBOOT,
            0,
            0,
            0,
            0,
            0);
}

void exec(const char* filename)
{
    syscall(SYSCALL_EXEC,
            (uint32_t)filename,
            0,
            0,
            0,
            0);
    panic("Exec failed");
}

void debug_writen(const char* str, size_t count)
{
    while(count) {
        outb(0xE9, *str);
        str++;
        count--;
    }
}

void debug_write(const char* str)
{
    while(*str) {
        outb(0xE9, *str);
        str++;
    }
}

void __log(const char* func, const char* file, int line, const char* fmt, ...)
{
    va_list args;

    unsigned char buffer[128];
    struct message* msg = (struct message*)buffer;
    char* msg_text = (char*)msg->data;

    va_start(args, fmt);
    vsnprintf(msg_text, sizeof(buffer) - sizeof(struct message), fmt, args);
    va_end(args);

    msg->reply_port = pcb.ack_port;
    msg->code = LoggerMessageTrace;
    msg->len = strlen((const char*)msg->data) + 1;

    int ret = msgsend(LoggerPort, msg);
    if(ret) {
        /* Logger must not be up, manually write to log */
        debug_write("Failed to send message to logger\n");
        debug_write("Message: ");
        debug_write(msg_text);
        debug_write("\n");
    }
}

void __assertion_failed(const char* function, const char* file, int line, const char* expression)
{
    __log(function, file, line, "Assertion failed: %s", expression);
    abort();
}

size_t initrd_get_size()
{
    char msg_buffer[128];
    struct message* msg = (struct message*)msg_buffer;
    msg->reply_port = pcb.ack_port;
    msg->code = KernelMessageInitrdGetSize;
    msg->len = 0;

    int ret = msgsend(KernelPort, msg);
    if(ret == -1)
        return -1;

    ret = msgrecv(pcb.ack_port, msg, sizeof(msg_buffer), NULL);
    if(ret != 0)
        return -1;
    else if(msg->code != KernelMessageResult)
        return -1;

    struct deserializer deserializer;
    deserializer_init(&deserializer, msg->data, msg->len);
    size_t result = deserialize_size_t(&deserializer);
    return result;
}

int initrd_read(void* dest, size_t size, size_t offset)
{
    char msg_buffer[512 + sizeof(struct message)];
    struct message* msg = (struct message*)msg_buffer;
    msg->reply_port = pcb.ack_port;
    msg->code = KernelMessageInitrdRead;
    
    struct serializer serializer;
    serializer_init(&serializer, msg->data, sizeof(msg_buffer) - sizeof(struct message));

    /* Substract a size_t because data also includes the serialized size_t */
    if(size > sizeof(msg_buffer) - sizeof(struct message) - sizeof(size_t))
        size = sizeof(msg_buffer) - sizeof(struct message) - sizeof(size_t);

    serialize_size_t(&serializer, size);
    serialize_size_t(&serializer, offset);
    msg->len = serializer_finish(&serializer);

    int ret = msgsend(KernelPort, msg);
    if(ret == -1)
        return -1;

    unsigned required_size;
    ret = msgrecv(pcb.ack_port, msg, sizeof(msg_buffer), &required_size);
    if(ret != 0) {
        trace("initrd_read() failed. Need %d bytes, have %d",
              required_size, sizeof(msg_buffer));
        return -1;
    } else if(msg->code != KernelMessageResult)
        return -1;

    struct deserializer deserializer;
    deserializer_init(&deserializer, msg->data, msg->len);
    ret = deserialize_int(&deserializer);
    if(ret == -1)
        return -1;

    const void* src_buf = deserialize_buffer(&deserializer, (size_t)ret);
    memcpy(dest, src_buf, ret);
    return ret;
}

bool get_task_info(int pid, struct task_info* buffer)
{
    unsigned char msg_buffer[sizeof(struct message) + sizeof(struct task_info) + sizeof(size_t)];
    struct message* msg = (struct message*)msg_buffer;
    msg->reply_port = pcb.ack_port;
    msg->code = KernelMessageGetTaskInfo;
    
    struct serializer serializer;
    serializer_init(&serializer, msg->data, sizeof(msg_buffer) - sizeof(struct message));
    serialize_int(&serializer, pid);
    msg->len = serializer_finish(&serializer);

    int ret = msgsend(KernelPort, msg);
    if(ret)
        return -1;

    ret = msgrecv(pcb.ack_port, msg, sizeof(msg_buffer), NULL);
    if(ret)
        return false;
    else if(msg->code != KernelMessageResult)
        return -1;

    struct deserializer deserializer;
    deserializer_init(&deserializer, msg->data, msg->len);

    ret = deserialize_int(&deserializer);
    if(ret)
        return -1;

    const struct task_info* info = deserialize_buffer(&deserializer, sizeof(struct task_info));
    memcpy(buffer, info, sizeof(struct task_info));
    return true;
}

void* mmap(void* addr, size_t size, uint32_t flags)
{
    uint32_t result = syscall(SYSCALL_MMAP,
                              (uint32_t)addr,
                              (uint32_t)size,
                              flags,
                              0,
                              0);
    return (void*)result;
}

int munmap(void* addr)
{
    uint32_t result = syscall(SYSCALL_MUNMAP,
                              (uint32_t)addr,
                              0,
                              0,
                              0,
                              0);
    return (int)result;
}

int open(const char* filename, unsigned flags, int mode)
{
    /* Send a VFSMessageOpen to VSFPort and wait for reply */
    unsigned char buffer[MAX_PATH + sizeof(struct vfs_open_data) + sizeof(struct message)];

    struct message* msg = (struct message*)buffer;
    msg->reply_port = pcb.ack_port;
    msg->code = VFSMessageOpen;
    msg->len = sizeof(struct vfs_open_data) + strlen(filename) + 1;

    struct vfs_open_data* open_data = (struct vfs_open_data*)&msg->data;
    open_data->mode = flags;
    open_data->perm = mode;
    strlcpy(open_data->filename, filename, MAX_PATH);

    int ret = msgsend(VFSPort, msg);
    if(ret)
        return -1;

    ret = msgrecv(pcb.ack_port, msg, sizeof(buffer), NULL);
    if(ret != 0)
        return -1;

    assert(msg->code == VFSMessageResult);

    struct vfs_result_data* result_data = (struct vfs_result_data*)msg->data;
    return result_data->result;
}

int read(int fd, void* buffer, size_t size)
{
    unsigned char msg_buffer[sizeof(struct vfs_result_data) + sizeof(struct message) + 512];

    struct message* msg = (struct message*)msg_buffer;
    msg->reply_port = pcb.ack_port;
    msg->code = VFSMessageRead;
    msg->len = sizeof(struct vfs_read_data) + sizeof(struct vfs_read_data);

    struct vfs_read_data* read_data = (struct vfs_read_data*)&msg->data;
    read_data->fd = fd;
    read_data->size = size;

    int ret = msgsend(VFSPort, msg);
    if(ret) {
        trace("msgsend() failed");
        return -1;
    }

    ret = msgrecv(pcb.ack_port, msg, sizeof(msg_buffer), NULL);
    if(ret != 0) {
        trace("msgrecv() failed");
        return -1;
    }

    assert(msg->code == VFSMessageResult);

    struct vfs_result_data* result_data = (struct vfs_result_data*)msg->data;
    assert(result_data->result <= size);
    memcpy(buffer, result_data->data, result_data->result);
    return result_data->result;
}

void main();
void runtime_entry()
{
    pcb.ack_port = port_open(-1);
    main();
    exit();
}



