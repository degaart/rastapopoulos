#include <stddef.h>
#include <port.h>

#include "kernel_task_client.h"
#include "runtime.h"

void reboot()
{
    kernel_reboot(KernelPort, pcb.ack_port);
}

