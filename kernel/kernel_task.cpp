#include "kernel_task.h"
#include "process.h"

void KernelTask::entry() {
    TRACE("Helloooooo. I'm kernel_task, nice to meet you");

    while(true) {
        yield();
        TRACE("Still here");
    }
}

