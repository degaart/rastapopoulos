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

#include "kernel_task_server.h"

int handle_kernel_get_task_info(int sender_pid, int pid, /* out */ void* buffer, /* in, out */ size_t* buffer_size)
{
    if(*buffer_size < sizeof(struct task_info))
        return -1;

    struct task_info* ti = buffer;

    enter_critical_section();
    bool success = get_task_info(ti, pid);
    leave_critical_section();

    if(!success) {
        return -1;
    } else {
        *buffer_size = sizeof(struct task_info);
        return 0;
    }
}

void handle_kernel_reboot(int sender_pid)
{
    trace("Reboot requested by pid %d", sender_pid);
    reboot();
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

    // Dispatch messages
    rpc_dispatch(KernelPort);
    panic("Invalid code path");
}




