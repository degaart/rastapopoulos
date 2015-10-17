#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdint.h>
#include "pagedir.h"

class Process {
private:
    uint32_t _pid;
    Pagedir* _pagedir;
    uint8_t _kernel_stack[0x1000];      /* 4k kernel stack */
public:
    Process(uint32_t pid);
    ~Process();
    void execute();
};

#endif


