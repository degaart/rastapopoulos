#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdint.h>
#include "pagedir.h"
#include "regs.h"
#include "idt.h"

class Process {
public:
    enum Ring {
        RING0,
        RING1,
        RING2,
        RING3
    };

    const uint32_t  USER_STACK_END = 0xBFFFFFFF;    /* Last useable byte of user stack */
    const uint32_t  PROCESS_ENTRY = 0x400000;       /* 4mb mark */

private:
    uint32_t        _pid;
    char            _name[32];

    Pagedir*        _pagedir;
    uint8_t         _kernel_stack[0x1000];      /* 4k kernel stack (in kernel-space) */
    uint8_t*        _user_stack;                /* start of user stack (in this process's address space). Top of stack is always USER_STACK_END */
    Ring            _current_ring;
    uint32_t        _kernel_esp;
    regs_t          _regs;

    static Process* _processes[100];            /* FUCK THE POLICE! */
    static uint32_t _process_count;
    static uint32_t _current_pid;
    static Process* _current_process;
    static void     switch_process(Process* proc);
    static void     resume_next_process(void* args, const isr_regs_t* regs);

    Process(uint32_t pid, const char* name);
    ~Process();
    Process(const Process&) = delete;
    Process(const Process&&) = delete;
    Process& operator=(const Process&) = delete;
    Process& operator=(const Process&&) = delete;

public:
    static void init();
    static Process* create(const char* name);

    void resume();
};

#endif


