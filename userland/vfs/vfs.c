#include "runtime.h"
#include "debug.h"
#include "port.h"
#include "vfs.h"
#include "fat/ff.h"
#include <string.h>
#include <util.h>
#include <malloc.h>
#include <serializer.h>

/*
 * Primitive VFS
 * Only one fs supported: initrd
 * And it is implemented in the stupidest way possible
 */
struct tar_header {
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag[1];
};
static const struct tar_header* initrd;

struct descriptor {
    list_declare_node(descriptor) node;
    int fd;
    size_t pos;
    uint32_t mode;
    FIL hfile;
};
list_declare(descriptor_list, descriptor);

struct procinfo {
    list_declare_node(procinfo) node;
    int pid;
    int next_fd;
    struct descriptor_list descriptors;
};
list_declare(procinfo_list, procinfo);

static struct procinfo_list proc_infos = {0};

#define INVALID_FD (-1)
#define INVALID_PID (-1)

#define MessageHandler(msgcode) \
    static void handle_ ## msgcode (struct message* msg, struct deserializer* args, struct serializer* result)
#define MessageCase(msg) \
    case msg: handle_ ## msg (recv_buf, &args, &results); break

static int next_fd(struct procinfo* pi)
{
    int result = pi->next_fd++;
    assert(result >= 0);
    return result;
}

static struct descriptor* descriptor_create(struct procinfo* pi)
{
    assert(pi);

    struct descriptor* desc = malloc(sizeof(struct descriptor));
    memset(desc, 0, sizeof(struct descriptor));
    desc->fd = next_fd(pi);

    list_append(&pi->descriptors, desc, node);

    return desc;
}

static struct descriptor* descriptor_get(struct procinfo* pi, int fd)
{
    assert(pi);

    struct descriptor* result = NULL;
    list_foreach(descriptor, desc, &pi->descriptors, node) {
        if(desc->fd == fd) {
            result = desc;
            break;
        }
    }
    return result;
}

static void descriptor_destroy(struct procinfo* pi, struct descriptor* desc)
{
    assert(pi != NULL);
    assert(desc != NULL);

    list_remove(&pi->descriptors,
                desc,
                node);
    free(desc);
}

static struct procinfo* procinfo_create(int pid)
{
    assert(pid != INVALID_PID);

    /* Assert that it does not already exists */
    list_foreach(procinfo, pi, &proc_infos, node) {
        if(pi->pid == pid) {
            assert(!"procinfo already exists");
        }
    }

    struct procinfo* pi = malloc(sizeof(struct procinfo));
    memset(pi, 0, sizeof(struct procinfo));
    pi->pid = pid;
    list_init(&pi->descriptors);

    list_append(&proc_infos, pi, node);

    return pi;
}

static struct procinfo* procinfo_get(int pid)
{
    assert(pid != INVALID_PID);

    struct procinfo* result = NULL;
    list_foreach(procinfo, pi, &proc_infos, node) {
        if(pi->pid == pid) {
            result = pi;
            break;
        }
    }
    return result;
}

static unsigned getsize(const char *in)
{
    unsigned int size = 0;
    unsigned int j;
    unsigned int count = 1;

    for (j = 11; j > 0; j--, count *= 8)
        size += ((in[j - 1] - '0') * count);

    return size;
}

/*************************************************************************
 * API functions implementations
 *************************************************************************/
static int open_fn(struct procinfo* pinfo,
                   const char* filename, int mode, int perm)
{
    assert(mode == O_RDONLY); /* no write support for now */

    FIL hfile;
    FRESULT fres = f_open(&hfile, filename, FA_READ);
    if(fres != FR_OK) {
        return -1;
    }

    struct descriptor* desc = descriptor_create(pinfo);
    assert(desc != NULL);

    desc->pos = 0;
    desc->mode = mode;
    memcpy(&desc->hfile, &hfile, sizeof(hfile));

    return desc->fd;
}

static int close_fn(struct procinfo* pinfo,
                    int fd)
{
    struct descriptor* desc = descriptor_get(pinfo, fd);
    assert(desc);
    assert(desc->fd == fd);

    f_close(&desc->hfile);
    memset(&desc->hfile, 0, sizeof(desc->hfile));

    descriptor_destroy(pinfo, desc);
    return 0;
}

static int read_fn(struct procinfo* pinfo,
                   int fd, void* buffer, size_t size)
{
    if(fd == INVALID_FD)
        return -1;
    else if(size == 0)
        return 0;

    struct descriptor* desc = descriptor_get(pinfo, fd);
    if(!desc)
        return -1;

    unsigned read_bytes;
    FRESULT fres = f_read(&desc->hfile,
                          buffer,
                          size,
                          &read_bytes);
    if(fres != FR_OK)
        return -1;
    
    return (int)read_bytes;
}

static int write_fn(int fd, const void* buffer, size_t size)
{
    return -1;
}


/*************************************************************************
 * API functions wrappers
 *************************************************************************/
MessageHandler(VFSMessageOpen)
{
    uint32_t mode = deserialize_int(args);
    uint32_t perm = deserialize_int(args);
    int filename_len = deserialize_int(args);
    const char* filename = deserialize_buffer(args, filename_len);

    struct procinfo* pi = procinfo_get(msg->sender);
    if(!pi) {
        pi = procinfo_create(msg->sender);
    }
    assert(pi != NULL);

    int fd = open_fn(pi, filename, mode, perm);
    serialize_int(result, fd);
}

MessageHandler(VFSMessageClose)
{
    int fd = deserialize_int(args);

    struct procinfo* pi = procinfo_get(msg->sender);
    assert(pi != NULL);

    int retcode = close_fn(pi, fd);
    serialize_int(result, retcode);
}

MessageHandler(VFSMessageRead)
{
    int fd = deserialize_int(args);
    size_t size = deserialize_size_t(args);

    struct procinfo* pi = procinfo_get(msg->sender);
    assert(pi != NULL);

    int* retcode = serialize_int(result, -1);
    size_t max_buffer_size;
    void* buffer = serialize_buffer(result, &max_buffer_size);
    if(max_buffer_size < size) {
        size = max_buffer_size;
    }

    int read_size = read_fn(pi,
                            fd,
                            buffer,
                            size);
    if(read_size > 0)
        serialize_buffer_finish(result, read_size);
    else
        serialize_buffer_finish(result, 0);

    *retcode = read_size;
}

MessageHandler(VFSMessageWrite)
{
    int fd = deserialize_int(args);
    size_t size = deserialize_size_t(args);
    const void* buffer = deserialize_buffer(args, size);

    struct procinfo* pi = procinfo_get(msg->sender);
    assert(pi != NULL);

    int* retcode = serialize_int(result, -1);
    *retcode = write_fn(fd, buffer, size);
}

void main()
{
    /* Open our port */
    trace("Opening VFS port");
    int ret = port_open(VFSPort);
    if(ret < 0) {
        panic("Failed to open VFS port");
    }

    /* Mount volume */
    FATFS fat = {0};
    FRESULT fres = f_mount(&fat, "0:/", 1);
    if(fres != FR_OK) {
        panic("mount() failed: %d", fres);
    }

    /* Alloc memory for receiving and sending messages */
    trace("Prepare buffers");
    struct message* recv_buf = malloc(4096);
    struct message* snd_buf = malloc(4096);

    /* Begin processing messages */
    trace("VFS started");
    list_init(&proc_infos);

    while(1) {
        unsigned outsiz;
        int ret = msgrecv(VFSPort, 
                          recv_buf, 
                          4096, 
                          &outsiz);
        if(ret != 0) {
            panic("msgrecv failed");
        }

        struct deserializer args;
        deserializer_init(&args, recv_buf->data, recv_buf->len);

        struct serializer results;
        serializer_init(&results, snd_buf->data, 4096 - sizeof(struct message));

        int out_size;
        struct vfs_result_data* out = (struct vfs_result_data*)
            ((unsigned char*)snd_buf + sizeof(struct message));
        memset(snd_buf, 0, 4096);
        switch(recv_buf->code) {
            MessageCase(VFSMessageOpen);
            MessageCase(VFSMessageClose);
            MessageCase(VFSMessageRead);
            MessageCase(VFSMessageWrite);
            default:
                panic("Unhandled message: %d", recv_buf->code);
        }

        snd_buf->len = serializer_finish(&results);
        snd_buf->reply_port = INVALID_PORT;
        snd_buf->code = VFSMessageResult;

        ret = msgsend(recv_buf->reply_port, snd_buf);
        if(ret) {
            panic("msgsend() failed");
        }
    }
}


