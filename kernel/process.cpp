#include "process.h"
#include "string.h"
#include "util.h"
#include "debug.h"
#include "vmm.h"
#include "gdt.h"

extern "C"
void switch_to_usermode();

Process::Process(uint32_t pid)
: _pid(pid) {
    _pagedir = VMM::new_pagedir();
    memset(_kernel_stack, 0xCC, sizeof(_kernel_stack));
}

Process::~Process() {
    VMM::free_pagedir(_pagedir);
}

void Process::execute() {
    GDT::set_kernel_stack(_kernel_stack);
    switch_to_usermode();
}
