#include "runtime.h"
#include "debug.h"
#include "port.h"
#include "vfs.h"
#include <string.h>
#include <util.h>

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

struct filedesc {
    int pid;
    size_t pos;
    uint32_t mode;
    unsigned char* data;
    size_t size;
};
static struct filedesc file_descriptors[32];
static int next_fd = 0;

#define invalid_code_path() \
    panic("Invalid code path")

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
 * Open specified file
 * Message:
 *  struct vfs_open_data {
 *      uint32_t mode;
 *      uint32_t perm;
 *      char filename[];
 *  }
 * Results:
 *  -1      Error
 *  else    File descriptor
 */
static int handle_open(const struct message* msg, struct vfs_result_data* result_buffer)
{
    const struct vfs_open_data* data = (const struct vfs_open_data*)msg->data;
    assert(data->mode == O_RDONLY); /* no write support for now */
    
    //trace("open(%s, 0x%X, 0x%X)", data->filename, data->mode, data->perm);
    const struct tar_header* hdr = initrd;
    while(true) {
        if(hdr->filename[0] == 0)
            break;

        unsigned size = getsize(hdr->size);
        if(!strcmp(hdr->filename, data->filename)) {
            /* Create descriptor */
            int fd_index = next_fd++;
            assert(fd_index < sizeof(file_descriptors) / sizeof(file_descriptors[0]));

            struct filedesc* fd = &file_descriptors[fd_index];
            fd->pid = msg->sender;
            fd->pos = 0;
            fd->mode = data->mode;
            fd->data = (unsigned char*)hdr + 512;
            fd->size = size;

            result_buffer->result = fd_index;
            return sizeof(uint32_t);
        }

        hdr = (const struct tar_header*)
            ((unsigned char*)hdr + 512 + ALIGN(size, 512));
    }
    result_buffer->result = -1;
    return sizeof(uint32_t);
}

static int handle_close(const struct message* msg, struct vfs_result_data* result_buffer)
{
    assert(!"Not implemented yet");
    return 0;
}

static int handle_read(const struct message* msg, struct vfs_result_data* result_buffer)
{
    const struct vfs_read_data* read_data = (const struct vfs_read_data*)msg->data;
    //trace("VFSMessageRead(%d, %d)", read_data->fd, (int)read_data->size);
    struct filedesc* fd = &file_descriptors[read_data->fd];

    size_t remaining = fd->size - fd->pos;
    if(remaining == 0) {
        result_buffer->result = 0;
        return sizeof(uint32_t);
    }

    size_t size = read_data->size;
    if(size > remaining)
        size = remaining;

    memcpy(result_buffer->data, 
           fd->data + fd->pos,
           size);

    fd->pos += size;
    result_buffer->result = size;
    return sizeof(uint32_t) + size;
}

static int handle_write(const struct message* msg, struct vfs_result_data* result_buffer)
{
    assert(!"Not implemented yet");
    return 0;
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
    unsigned char* heap_start = (unsigned char*)ALIGN(_END_,4096);
    void* mmap_ret = mmap(heap_start, ALIGN(initrd_size, 4096), PROT_READ|PROT_WRITE); 
    assert(mmap_ret != NULL);

    trace("Copying initrd into our address-space");
    ret = initrd_copy(mmap_ret, initrd_size);
    assert(ret != -1);

    initrd = (struct tar_header*)heap_start;

    /* Alloc memory for receiving and sending messages */
    trace("Prepare buffers");
    heap_start = ALIGN((unsigned char*)mmap_ret + ALIGN(initrd_size, 4096), 4096);
    struct message* recv_buf = (struct message*)mmap(heap_start, 4096, PROT_READ|PROT_WRITE);
    assert(recv_buf != NULL);

    heap_start = (unsigned char*)recv_buf + 4096;
    struct message* snd_buf = (struct message*)mmap(heap_start, 4096, PROT_READ|PROT_WRITE);
    assert(snd_buf != NULL);

    /* Begin processing messages */
    trace("VFS started");

    while(1) {
        unsigned outsiz;
        int ret = msgrecv(VFSPort, 
                          recv_buf, 
                          4096, 
                          &outsiz);
        if(ret != 0) {
            panic("msgrecv failed");
        }

        int out_size;
        struct vfs_result_data* out = (struct vfs_result_data*)
            ((unsigned char*)snd_buf + sizeof(struct message));
        memset(snd_buf, 0, 4096);
        switch(recv_buf->code) {
            case VFSMessageOpen:
                out_size = handle_open(recv_buf, out);
                break;
            case VFSMessageClose:
                out_size = handle_close(recv_buf, out);
                break;
            case VFSMessageRead:
                out_size = handle_read(recv_buf, out);
                break;
            case VFSMessageWrite:
                out_size = handle_write(recv_buf, out);
                break;
        }

        snd_buf->code = VFSMessageResult;
        snd_buf->len = out_size;

        ret = msgsend(recv_buf->reply_port, snd_buf);
        if(ret != 0) {
            panic("msgsend failed");
        }
    }
}


