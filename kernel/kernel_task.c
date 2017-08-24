#include "kernel_task.h"

#include "debug.h"
#include "port.h"
#include "syscall.h"
#include "locks.h"
#include "scheduler.h"
#include "initrd.h"
#include "elf.h"
#include "serializer.h"
#include "pmm.h"
#include "task_info.h"

static void handle_initrd_get_size(struct deserializer* args,
                                   struct serializer* result)
{
    int size = initrd_get_size();
    int* ptr = serialize_int(result, size);
    assert(*ptr == size);                   /* just debugging. Can be removed */
}

static void handle_initrd_read(struct deserializer* args,
                               struct serializer* result)
{
    size_t size = deserialize_size_t(args);
    size_t offset = deserialize_size_t(args);

    int* intret = serialize_int(result, -1);

    size_t buffer_size;
    void* buffer = serialize_buffer(result, &buffer_size);
    if(buffer_size == 0) {
        serialize_buffer_finish(result, 0);
        *intret = -1;
        return;
    } else if(buffer_size < size) {
        size = buffer_size;
    }

    int ret = initrd_read(buffer, size, offset);
    if(ret == -1) {
        serialize_buffer_finish(result, 0);
        *intret = -1;
        return;
    }

    serialize_buffer_finish(result, ret);
    *intret = ret;
}

static void handle_get_task_info(struct deserializer* args,
                                 struct serializer* result)
{
    int pid = deserialize_int(args);

    int* ret = serialize_int(result, -1);
    size_t buffer_size;
    struct task_info* buffer = serialize_buffer(result, &buffer_size);
    if(buffer_size < sizeof(struct task_info)) {
        serialize_buffer_finish(result, 0);
        return;
    }

    enter_critical_section();
    bool fret = get_task_info(buffer, pid);
    leave_critical_section();

    if(!fret) {
        serialize_buffer_finish(result, 0);
        return;
    }

    *ret = 0;
    serialize_buffer_finish(result, sizeof(struct task_info));
}

void kernel_task_entry()
{
    trace("kernel_task started");

    // Open kernel_task port
    int ret = port_open(KernelPort);
    if(ret == INVALID_PORT) {
        panic("Failed to open KernelPort");
    }

    // start init
    int pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
    if(!pid) {
        enter_critical_section();
        current_task_set_name("init.elf");
        leave_critical_section();

        const struct initrd_file* init_file = initrd_get_file("init.elf");
        assert(init_file != NULL);

        elf_entry_t entry = load_elf(init_file->data, init_file->size);
        jump_to_usermode(entry);
    }

    // alloc memory for messages
    uint32_t frame = pmm_alloc();
    struct message* recv_buf = (struct message*)0x400000;
    vmm_map(recv_buf, frame, VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE);
    
    frame = pmm_alloc();
    struct message* snd_buf = (struct message*)(0x400000 + PAGE_SIZE);
    vmm_map(snd_buf, frame, VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE);

    // Wait for messages
    while(true) {
        unsigned outsize;
        int ret = msgrecv(KernelPort, recv_buf, PAGE_SIZE, &outsize);
        if(ret) {
            panic("msgrecv failed");
        }

        struct deserializer deserializer;
        deserializer_init(&deserializer, recv_buf->data, recv_buf->len);
        
        struct serializer serializer;
        serializer_init(&serializer, snd_buf->data, PAGE_SIZE - sizeof(struct message));

        switch((enum KernelMessages)recv_buf->code) {
            case KernelMessageInitrdGetSize:
                handle_initrd_get_size(&deserializer, &serializer);
                break;
            case KernelMessageInitrdRead:
                handle_initrd_read(&deserializer, &serializer);
                break;
            case KernelMessageGetTaskInfo:
                handle_get_task_info(&deserializer, &serializer);
                break;
            default:
                panic("Invalid message code");
        }

        size_t datalen = serializer_finish(&serializer);
        snd_buf->reply_port = INVALID_PORT;
        snd_buf->code = KernelMessageResult;
        snd_buf->len = datalen;

        ret = msgsend(recv_buf->reply_port, snd_buf);
        if(ret)
            panic("msgsend() failed");
    }
}




