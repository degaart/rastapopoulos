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
#include "malloc.h"
#include "util.h"
#include "../blockdrv/blockdrv.h"

struct pcb pcb;
int errno = 0;

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
    struct message msg;
    msg.reply_port = INVALID_PORT;
    msg.len = 0;
    msg.code = KernelMessageReboot;

    int ret = msgsend(KernelPort, &msg);
    if(ret) {
        panic("Reboot failed");
    }
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

    size_t buffer_size = sizeof(buffer) - sizeof(struct message);
    int written = snprintf(msg_text, buffer_size, "[%s:%d] ", basename(file), line);
    assert(written <= buffer_size);

    buffer_size -= written - 1;
    msg_text += written - 1;

    va_start(args, fmt);
    vsnprintf(msg_text, buffer_size, fmt, args);
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
    char msg_buffer[sizeof(struct message) + sizeof(size_t)];
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
    size_t buffer_size = 512 + sizeof(struct message);
    struct message* msg = malloc(buffer_size);
    msg->reply_port = pcb.ack_port;
    msg->code = KernelMessageInitrdRead;
    
    struct serializer serializer;
    serializer_init(&serializer, msg->data, buffer_size - sizeof(struct message));

    /* Substract a size_t because data also includes the serialized size_t */
    if(size > buffer_size - sizeof(struct message) - sizeof(size_t))
        size = buffer_size - sizeof(struct message) - sizeof(size_t);

    serialize_size_t(&serializer, size);
    serialize_size_t(&serializer, offset);
    msg->len = serializer_finish(&serializer);

    int ret = msgsend(KernelPort, msg);
    if(ret == -1) {
        free(msg);
        return -1;
    }

    unsigned required_size;
    ret = msgrecv(pcb.ack_port, msg, buffer_size, &required_size);
    if(ret != 0) {
        trace("initrd_read() failed. Need %d bytes, have %d",
              required_size, buffer_size);
        free(msg);
        return -1;
    } else if(msg->code != KernelMessageResult) {
        free(msg);
        return -1;
    }

    struct deserializer deserializer;
    deserializer_init(&deserializer, msg->data, msg->len);
    ret = deserialize_int(&deserializer);
    if(ret == -1) {
        free(msg);
        return -1;
    }

    const void* src_buf = deserialize_buffer(&deserializer, (size_t)ret);
    memcpy(dest, src_buf, ret);
    free(msg);
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

int open(const char* filename, unsigned mode, int perm)
{
    if(!filename || !mode)
        return -1;

    /* Send a VFSMessageOpen to VSFPort and wait for reply */
    size_t buffer_size = sizeof(struct message) +
        sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int) + strlen(filename) + 1;

    struct message* msg = malloc(buffer_size);
    msg->reply_port = pcb.ack_port;
    msg->code = VFSMessageOpen;

    struct serializer request;
    serializer_init(&request, msg->data, buffer_size - sizeof(struct message));
    serialize_int(&request, mode);
    serialize_int(&request, perm);
    serialize_int(&request, strlen(filename) + 1);

    size_t filename_buffer_size;
    char* filename_buffer = serialize_buffer(&request, &filename_buffer_size);
    strlcpy(filename_buffer, filename, filename_buffer_size);
    serialize_buffer_finish(&request, strlen(filename) + 1);

    size_t msg_len = serializer_finish(&request);
    assert(msg_len <= buffer_size);

    msg->len = msg_len;

    int ret = msgsend(VFSPort, msg);
    if(ret) {
        free(msg);
        return -1;
    }

    ret = msgrecv(pcb.ack_port, msg, buffer_size, NULL);
    if(ret != 0) {
        free(msg);
        return -1;
    }

    assert(msg->code == VFSMessageResult);

    struct deserializer result;
    deserializer_init(&result, msg->data, msg->len);

    int retcode = deserialize_int(&result);
    free(msg);
    return retcode;
}

int read(int fd, void* buffer, size_t size)
{
    if(size == 0)
        return 0;
    else if(fd == -1)
        return -1;
    else if(buffer == NULL)
        return -1;

    size_t buffer_size = sizeof(struct message) + sizeof(int) + size;
    if(buffer_size < sizeof(struct message) + sizeof(int) + sizeof(size_t))
        buffer_size = sizeof(struct message) + sizeof(int) + sizeof(size_t);

    struct message* msg = malloc(buffer_size);
    msg->reply_port = pcb.ack_port;
    msg->code = VFSMessageRead;

    struct serializer request;
    serializer_init(&request, msg->data, buffer_size - sizeof(struct message));
    serialize_int(&request, fd);
    serialize_size_t(&request, size);
    msg->len = serializer_finish(&request);
    assert(msg->len <= buffer_size);

    int ret = msgsend(VFSPort, msg);
    if(ret) {
        free(msg);
        return -1;
    }

    ret = msgrecv(pcb.ack_port, msg, buffer_size, NULL);
    if(ret != 0) {
        free(msg);
        return -1;
    }

    assert(msg->code == VFSMessageResult);

    struct deserializer result;
    deserializer_init(&result, msg->data, msg->len);

    int retcode = deserialize_int(&result);
    if(retcode > 0) {
        assert(retcode <= size);

        const void* result_buffer = deserialize_buffer(&result, retcode);
        memcpy(buffer, result_buffer, retcode);
    }
    free(msg);
    return retcode;
}

int close(int fd)
{
    assert(fd != -1);

    size_t buffer_size = sizeof(struct message) + sizeof(int);
    struct message* msg = malloc(buffer_size);
    msg->reply_port = pcb.ack_port;
    msg->code = VFSMessageClose;

    struct serializer request;
    serializer_init(&request, msg->data, buffer_size - sizeof(struct message));
    serialize_int(&request, fd);
    msg->len = serializer_finish(&request);

    int ret = msgsend(VFSPort, msg);
    if(ret != 0) {
        free(msg);
        return -1;
    }

    ret = msgrecv(pcb.ack_port, msg, buffer_size, NULL);
    if(ret != 0) {
        free(msg);
        return -1;
    }

    assert(msg->code == VFSMessageResult);

    struct deserializer result;
    deserializer_init(&result, msg->data, msg->len);

    int retcode = deserialize_int(&result);
    free(msg);

    return retcode;
}

static unsigned char* program_break = 0;
void* sbrk(ptrdiff_t incr)
{
    if(!program_break)
        program_break = ALIGN(_END_, 4096);

    if(!incr) {
        return program_break;
    } else if(incr < 0) {
        panic("Not implemented yet");
    } else {
        incr = ALIGN(incr, 4096);
        void* result = mmap(program_break, incr, PROT_READ|PROT_WRITE);
        assert(result != NULL);

        program_break += incr;
        return result;
    }
}

int hwportopen(int port)
{
    int ret = syscall(SYSCALL_HWPORTOPEN,
                      port,
                      0,
                      0,
                      0,
                      0);
    return ret;
}

int blockdrv_read_sector(void* buffer, size_t size, uint32_t sector)
{
    if(size < 512)
        return -1;

    size_t msg_size = sizeof(struct message) + sizeof(uint32_t) + 512;
    struct message* msg = malloc(msg_size);
    msg->reply_port = pcb.ack_port;
    msg->code = BlockDrvMessageReadSector;

    struct serializer request;
    serializer_init(&request, msg->data, msg_size - sizeof(struct message));
    serialize_int(&request, sector);

    msg->len = serializer_finish(&request);

    int ret = msgsend(BlockDrvPort, msg);
    if(ret) {
        free(msg);
        return -1;
    }

    ret = msgrecv(pcb.ack_port, msg, msg_size, NULL);
    if(ret) {
        free(msg);
        return -1;
    }

    assert(msg->code == BlockDrvMessageResult);

    struct deserializer result;
    deserializer_init(&result, msg->data, msg->len);

    int retcode = deserialize_int(&result);
    if(retcode == 0) {
        memcpy(buffer, deserialize_buffer(&result, 512), 512);
    }
    free(msg);

    return retcode;
}

uint32_t blockdrv_sector_count()
{
    size_t buffer_size = sizeof(struct message) + sizeof(uint32_t);
    struct message* msg = malloc(buffer_size);
    msg->reply_port = pcb.ack_port;
    msg->code = BlockDrvMessageSectorCount;
    msg->len = 0;

    int ret = msgsend(BlockDrvPort, msg);
    if(ret) {
        free(msg);
        return (uint32_t)-1;
    }

    ret = msgrecv(pcb.ack_port, msg, buffer_size, NULL);
    if(ret) {
        free(msg);
        return (uint32_t)-1;
    }

    assert(msg->code == BlockDrvMessageResult);

    struct deserializer result;
    deserializer_init(&result, msg->data, msg->len);

    uint32_t sectorcount = deserialize_int(&result);
    free(msg);

    return sectorcount;
}

uint64_t blockdrv_total_size()
{
    size_t buffer_size = sizeof(struct message) + sizeof(uint64_t);
    struct message* msg = malloc(buffer_size);
    msg->reply_port = pcb.ack_port;
    msg->code = BlockDrvMessageTotalSize;
    msg->len = 0;

    int ret = msgsend(BlockDrvPort, msg);
    if(ret) {
        free(msg);
        return (uint64_t)-1;
    }

    ret = msgrecv(pcb.ack_port, msg, buffer_size, NULL);
    if(ret) {
        free(msg);
        return (uint64_t)-1;
    }

    assert(msg->code == BlockDrvMessageResult);
    assert(msg->len == sizeof(uint64_t));

    struct deserializer result;
    deserializer_init(&result, msg->data, msg->len);

    uint64_t total_size = deserialize_int64(&result);
    free(msg);

    return total_size;
}

int blockdrv_read(void* buffer, size_t size, uint64_t offset)
{
    size_t buffer_size = sizeof(struct message) + sizeof(int64_t) + sizeof(int) + size;
    struct message* msg = malloc(buffer_size);

    struct serializer request;
    serializer_init(&request, msg->data, buffer_size - sizeof(struct message));
    serialize_int64(&request, offset);
    serialize_int(&request, (int)size);

    msg->reply_port = pcb.ack_port;
    msg->code = BlockDrvMessageRead;
    msg->len = serializer_finish(&request);

    int ret = msgsend(BlockDrvPort, msg);
    if(ret) {
        free(msg);
        return -1;
    }

    ret = msgrecv(pcb.ack_port, msg, buffer_size, NULL);
    if(ret) {
        free(msg);
        return -1;
    }

    assert(msg->code == BlockDrvMessageResult);

    struct deserializer result;
    deserializer_init(&result, msg->data, msg->len);

    int retcode = deserialize_int(&result);
    if(retcode > 0) {
        assert(retcode <= size);

        const void* src = deserialize_buffer(&result, retcode);
        memcpy(buffer, src, retcode);
    }
    free(msg);
    return retcode;
}

void main();
void runtime_entry()
{
    pcb.ack_port = port_open(-1);
    main();
    exit();
}



