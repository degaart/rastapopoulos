#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdint.h>
#include "linked_list.h"
#include "pagedir.h"
#include "regs.h"
#include "idt.h"
#include "Port.h"
#include "vmm.h"

class KernelTask;

class Process {
    friend class KernelTask;
public:
    enum Ring {
        RING0,
        RING3
    };

    static const uint32_t   KERNEL_STACK_START = VMM::USERSPACE_END + 1 - VMM::PAGE_SIZE;
    static const uint32_t   KERNEL_STACK_END = VMM::USERSPACE_END;
    static const uint32_t   USER_STACK_END = KERNEL_STACK_START - 1;

    enum State {
        NEW,            /* Newly constructed, has no pagedir, stack, etc... */
        READY,          /* Ready to run */
        MSG_WAIT,       /* Waiting for a message */
        SLEEP,          /* Sleeping until a deadline */
        EXITED          /* Already exited, waiting for parent to reap */
    };
private:
    typedef LinkedList<Process*> ProcessList_t;

    uint32_t                _pid;
    char                    _name[32];

    Pagedir*                _pagedir;
    uint32_t                _kernel_stack;              /* 4k kernel stack (in this process's) address space */
    uint32_t                _user_stack;                /* start of user stack (in this process's address space). End of stack is always USER_STACK_END */
    Ring                    _current_ring;
    uint32_t                _kernel_esp;
    regs_t                  _regs;
    LinkedList<Port*>       _ports;                     /* Ports this process can read from */
    State                   _state;

    static ProcessList_t    _processes;
    static Process*         _current_process;
    static uint32_t         _current_pid;
    static uint32_t         _current_ephemeral_port;

    Process(uint32_t pid);
    Process(uint32_t pid, const Process& proc) = delete; /* Dangerous, because of Pagedir ownership issues */
    ~Process();
    Process(const Process&) = delete;
    Process(const Process&&) = delete;
    Process& operator=(const Process&) = delete;
    Process& operator=(const Process&&) = delete;
    LinkedList<Port*>::Iterator port_iterator(uint32_t port_number);

    static uint32_t next_pid();
    static Process* next_process();
    static void resume_next_process(void* args, const isr_regs_t* regs);
    static uint32_t ephemeral_port_number();
    static void int_fork(isr_regs_t* regs);

    static uint32_t syscall_port_open(uint32_t param0, uint32_t param1, uint32_t param2);
    static uint32_t syscall_port_close(uint32_t param0, uint32_t param1, uint32_t param2);
    static uint32_t syscall_port_send(uint32_t param0, uint32_t param1, uint32_t param2);
    static uint32_t syscall_port_read(uint32_t param0, uint32_t param1, uint32_t param2);
public:
    static void init();
    static Process* create();
    static void switch_process(Process* proc);
    static void exit_current_process();
    static Process* current_process();
    static Process* process_for_port(uint32_t port);

    void load_elf(const char* filename);
    
    const char* name();
    void set_name(const char* name);
    uint32_t pid() {
        return _pid;
    }


    /*
        Open port for reading
        port: port number to read from. INVALID_PORT to allocate an ephemeral port number
        returns:
            INVALID_PORT        Port already allocated to another process
            other               Ephemeral port number allocated to process
    */
    static const uint32_t INVALID_PORT = 0x0;
    uint32_t open_port(uint32_t port);                  
    bool close_port(uint32_t port);                              /* Close port */
    Port* get_port(uint32_t port_number);
    void check_readable_block(const void* buffer, size_t size);
    void check_writable_block(const void* buffer, size_t size);
    uint8_t* kernel_stack();
};

#endif


