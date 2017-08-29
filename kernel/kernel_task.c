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
#include "kernel.h"
#include "kmalloc.h"
#include "io.h"

#define MessageHandler(msg) \
    static void handle_ ## msg (struct deserializer* args, struct serializer* result)
#define MessageCase(msg) \
    case msg: handle_ ## msg (&deserializer, &serializer); break

MessageHandler(KernelMessageInitrdGetSize)
{
    int size = initrd_get_size();
    int* ptr = serialize_int(result, size);
    assert(*ptr == size);                   /* just debugging. Can be removed */
}

MessageHandler(KernelMessageInitrdRead)
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

MessageHandler(KernelMessageGetTaskInfo)
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

MessageHandler(KernelMessageReboot)
{
    trace("Reboot requested");
    reboot();
}

/*
 * http://www.osdever.net/tutorials/view/lba-hdd-access-via-pio
 * http://wiki.osdev.org/ATA_PIO_Mode
 */
static void test_ata()
{
    bool pri_controller = false;
    bool pri_master = false, pri_slave = false;

    // Detect controllers
    trace("Detecting");

    outb(0x1F3, 0x88);
    int ret = inb(0x1F3);
    if(ret == 0x88)
        pri_controller = true;
    trace("Primary IDE Controller: %s", pri_controller ? "true" : "false");
    assert(ret == 0x88);

    // Detect drives
    if(pri_controller) {

        outb(0x1F6, 0xA0);
        io_delay();
        io_delay();
        io_delay();
        io_delay();
        int status = inb(0x1F7);
        if(status & 0x40) {
            pri_master = true;
        }

        outb(0x1F6, 0xB0);
        io_delay();
        io_delay();
        io_delay();
        io_delay();
        status = inb(0x1F7);
        if(status & 0x40) {
            pri_slave = true;
        }

        trace("Primary master: %s", pri_master ? "true": "false");
        trace("Primary slave: %s", pri_slave ? "true": "false");
    }

    trace("Reading");
    assert(pri_controller && pri_master);

    uint64_t addr = 0;
    int drive = 0;
    unsigned char buffer[512];

    outb(0x1F1, 0x00);
    outb(0x1F2, 0x01);                      /* sector count, 1 sector=512 bytes */
    outb(0x1F3, (uint8_t)(addr & 0xFF));    /* low 8 bits of address */
    outb(0x1F4, (uint8_t)(addr >> 8));
    outb(0x1F5, (uint8_t)(addr >> 16));
    outb(0x1F6, (uint8_t)(0xE0|((drive & 1)<<4)|((addr & 0x0F) >> 24)));
    outb(0x1F7, 0x20);                      /* command */

    while (!(inb(0x1F7) & 0x08));           /* wait for drive to be ready */
    for(int i = 0; i < 256; i++) {
        int w = inw(0x1F0);
        ((uint16_t*)buffer)[i] = w;
    }

    assert(buffer[0] = 0xEB);
    assert(buffer[1] = 0x63);
    assert(buffer[2] = 0x90);
    assert(buffer[3] = 0x00);
    assert(buffer[510] = 0x55);
    assert(buffer[511] = 0xAA);
}

void kernel_task_entry()
{
    trace("kernel_task started");

#if 1
    trace("Testing ATA commands");
    test_ata();
    reboot();
#else
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
    struct message* recv_buf = kmalloc(4096);
    struct message* snd_buf = kmalloc(4096);

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
            MessageCase(KernelMessageInitrdGetSize);
            MessageCase(KernelMessageInitrdRead);
            MessageCase(KernelMessageGetTaskInfo);
            MessageCase(KernelMessageReboot);
            default:
                panic("Invalid message code: %d", recv_buf->code);
        }

        size_t datalen = serializer_finish(&serializer);
        snd_buf->reply_port = INVALID_PORT;
        snd_buf->code = KernelMessageResult;
        snd_buf->len = datalen;

        ret = msgsend(recv_buf->reply_port, snd_buf);
        if(ret)
            panic("msgsend() failed");
    }
#endif
}




