#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include <stdint.h>
#include "regs.h"
#include "vmm.h"

struct context_t {
    uint32_t cs;
    uint32_t ds;
    uint32_t ss;
    Pagedir* pagedir;
    struct regs_t regs;
};

void switch_context(context_t* ctx);

#endif //_CONTEXT_H_

