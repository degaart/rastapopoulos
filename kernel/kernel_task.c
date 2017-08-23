#include "debug.h"
#include "port.h"
#include "syscall.h"
#include "locks.h"
#include "scheduler.h"
#include "initrd.h"
#include "elf.h"

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

    // Wait for messages
    char buffer[512];
    struct message* msg = (struct message*)buffer;
    while(true) {
        unsigned outsize;
        int ret = msgrecv(KernelPort, msg, sizeof(buffer), &outsize);
        if(ret) {
            panic("msgrecv failed");
        }
        panic("Invalid message code");
    }
}




