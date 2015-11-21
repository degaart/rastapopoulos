#include "kernel_task.h"
#include "process.h"
#include "io.h"

extern "C" uint32_t call_int81(void);

void KernelTask::entry() {
    TRACE("Helloooooo. I'm kernel_task, I task it so you don't have to");

    // Fork: call int 0x81
    // Child receives 0 in eax
    // Parent receives child's pid
    uint32_t pid = call_int81();
    if(pid) {
        while(true) {
            yield();
            outb(0xE9, '*');
        }
    } else {
        pid = call_int81();
        if(pid) {
            while(true) {
                yield();
                outb(0xE9, '+');
            }
        } else {
            while(true) {
                yield();
                outb(0xE9, '.');
            }
        }
        
    }
}

