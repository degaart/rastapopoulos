#include "runtime.h"
#include "debug.h"
#include "port.h"
#include "vfs.h"
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
    unsigned char* data;
    size_t size;
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


/*
 * int open(mode, perm, filename);
 * Results:
 *  -1      Error
 *  else    File descriptor
 */
MessageHandler(VFSMessageOpen)
{
    uint32_t mode = deserialize_int(args);
    uint32_t perm = deserialize_int(args);
    int filename_len = deserialize_int(args);
    const char* filename = deserialize_buffer(args, filename_len);

    assert(mode == O_RDONLY); /* no write support for now */
    
    //trace("open(%s, 0x%X, 0x%X)", data->filename, data->mode, data->perm);
    const struct tar_header* hdr = initrd;
    while(true) {
        if(hdr->filename[0] == 0)
            break;

        unsigned size = getsize(hdr->size);
        if(!strcmp(hdr->filename, filename)) {
            struct procinfo* pi = procinfo_get(msg->sender);
            if(pi == NULL) {
                pi = procinfo_create(msg->sender);
                assert(pi != NULL);
            }

            struct descriptor* desc = descriptor_create(pi);
            assert(desc != NULL);

            desc->pos = 0;
            desc->mode = mode;
            desc->data = (unsigned char*)hdr + 512;
            desc->size = size;

            serialize_int(result, desc->fd);
        }

        hdr = (const struct tar_header*)
            ((unsigned char*)hdr + 512 + ALIGN(size, 512));
    }

    serialize_int(result, -1);
}

MessageHandler(VFSMessageClose)
{
    assert(!"Not implemented yet");
}

/*
 * {int, buffer} read(int fd, size_t size);
 */
MessageHandler(VFSMessageRead)
{
    int fd = deserialize_int(args);
    size_t size = deserialize_size_t(args);

    if(fd == INVALID_FD) {
        serialize_int(result, -1);
        return;
    } else if(size == 0) {
        serialize_int(result, 0);
        return;
    }

    struct procinfo* pi = procinfo_get(msg->sender);
    if(!pi) {
        serialize_int(result, -1);
        return;
    }

    struct descriptor* desc = descriptor_get(pi, fd);
    if(!desc) {
        serialize_int(result, -1);
        return;
    }

    size_t remaining = desc->size - desc->pos;
    if(remaining == 0) {
        serialize_int(result, 0);
        return;
    } else if(size > remaining) {
        size = remaining;
    }

    int* retcode = serialize_int(result, -1);
    size_t max_buffer_size;
    void* buffer = serialize_buffer(result, &max_buffer_size);
    if(max_buffer_size < size) {
        size = max_buffer_size;
    }

    memcpy(buffer, 
           desc->data + desc->pos,
           size);

    desc->pos += size;
    serialize_buffer_finish(result, size);
    *retcode = size;
}

MessageHandler(VFSMessageWrite)
{
    assert(!"Not implemented yet");
}

void main()
{
    /* Open our port */
    trace("Opening VFS port");
    int ret = port_open(VFSPort);
    if(ret < 0) {
        panic("Failed to open VFS port");
    }

    /* Load initrd by asking kernel to copy it into our address space */
    trace("Loading initrd");
    size_t initrd_size = initrd_get_size();
    size_t offset = 0;
    unsigned char* initrd_buffer = malloc(initrd_size);
    initrd = (struct tar_header*)initrd_buffer;

    while(offset < initrd_size) {
        ret = initrd_read(initrd_buffer, initrd_size - offset, offset);
        if(ret == -1)
            panic("Failed to read initrd");
        else if(!ret)
            break;

        offset += ret;
        initrd_buffer += ret;
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


