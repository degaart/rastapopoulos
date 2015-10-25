#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdint.h>
#include "linked_list.h"
#include "pagedir.h"
#include "regs.h"
#include "idt.h"
#include "Port.h"

class Process {
public:
    enum Ring {
        RING0,
        RING3
    };

    static const uint32_t   USER_STACK_END = 0xBFFFFFFF;    /* Last useable byte of user stack */
    static const uint32_t   PROCESS_ENTRY = 0x400000;       /* 4mb mark */

private:
    typedef LinkedList<Process*> ProcessList_t;

    uint32_t                _pid;
    char                    _name[32];

    Pagedir*                _pagedir;
    uint8_t                 _kernel_stack[0x1000];      /* 4k kernel stack (in kernel-space) */
    uint32_t                _user_stack;                /* start of user stack (in this process's address space). End of stack is always USER_STACK_END */
    uint32_t                _workingset_size;           /* Size of process's working-set */
    Ring                    _current_ring;
    uint32_t                _kernel_esp;
    regs_t                  _regs;
    LinkedList<Port*>       _ports;                     /* Ports this process can read from */

    static ProcessList_t    _processes;
    static Process*         _current_process;
    static uint32_t         _current_pid;
    static uint32_t         _current_ephemeral_port;

    Process(uint32_t pid, const char* name);
    ~Process();
    Process(const Process&) = delete;
    Process(const Process&&) = delete;
    Process& operator=(const Process&) = delete;
    Process& operator=(const Process&&) = delete;
    LinkedList<Port*>::Iterator port_iterator(uint32_t port_number);

    static Process* next_process();
    static void resume_next_process(void* args, const isr_regs_t* regs);
    static uint32_t ephemeral_port_number();
public:
    static void init();
    static Process* create(const char* name);
    static void switch_process(Process* proc);
    static void exit_current_process();
    static Process* current_process();
    static Process* process_for_port(uint32_t port);

    void load_image(const char* filename);          /* Load process image from Initrd */
    const char* name();

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
};

#endif


