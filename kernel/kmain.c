#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kernel.h"
#include "io.h"
#include "halt.h"
#include "debug.h"
#include "reboot.h"
#include "string.h"
#include "registers.h"
#include "gdt.h"
#include "idt.h"
#include "kmalloc.h"
#include "multiboot.h"
#include "pmm.h"
#include "bitset.h"
#include "vmm.h"
#include "pic.h"
#include "pit.h"
#include "initrd.h"
#include "elf.h"

static void int80_handler(const struct isr_regs* regs)
{
    trace("Hello from int80");
}

static
void keyboard_handler(int irq, const struct isr_regs* regs)
{
    trace("keyboard_handler");
}

/*
 * task control block
 * WARNING: Update task_switch.asm after each layout change
 */
struct task {
    /* These fields are accessed by task_switch(), do not change order */
    unsigned char* esp;
    unsigned long cr3;

    uint32_t* pagedir;
    unsigned char* stack_bottom;
    struct process* next;
    unsigned state;
    char name[64];
};
struct task* current_task;
struct task* tasks[2];
extern unsigned char stack_bottom[];
extern void task_switch(struct task* next);

static
void init_multitasking()
{
    current_task = kmalloc(sizeof(struct task));
    bzero(current_task, sizeof(struct task));
    strlcpy(current_task->name, "KERNEL_TASK", sizeof(current_task->name));
    current_task->esp = 0;          /* will be filled by task_switch */
    current_task->stack_bottom = stack_bottom;
    current_task->pagedir = vmm_current_pagedir();
    current_task->cr3 = vmm_get_frame(current_task->pagedir);
    trace("task[0]->cr3: %p", current_task->cr3);

    tasks[0] = current_task;
}

static struct task* task_create(const char* name, void (*entry)())
{
    struct task* task = kmalloc(sizeof(struct task));
    bzero(task, sizeof(struct task));
    strlcpy(task->name, name, sizeof(task->name));
    task->stack_bottom = kmalloc_aligned(PAGE_SIZE, 16);

    uint32_t* stack = (uint32_t*)task->stack_bottom;
    stack[1023] = (uint32_t)entry;
    stack[1022] = 0xABCD0001;       /* ebp */
    stack[1021] = 0xABCD0002;       /* ebx */
    stack[1020] = 0xABCD0003;       /* esi */
    stack[1019] = 0xABCD0004;       /* edi */
    task->esp = (unsigned char*)&stack[1019];
    task->pagedir = vmm_create_pagedir();
    task->cr3 = vmm_get_frame(task->pagedir);
    trace("task[1]->cr3: %p", task->cr3);
    return task;
}

static
void task1_entry()
{
    while(1) {
        trace("task1 running");
        vmm_copy_kernel_mappings(tasks[1]->pagedir);
        task_switch(tasks[1]);
    }
}

static
void task2_entry()
{
    while(1) {
        trace("task2 running");
        vmm_copy_kernel_mappings(tasks[0]->pagedir);
        task_switch(tasks[0]);
    }
}

extern uint32_t initial_pagedir[];
void kmain(const struct multiboot_info* multiboot_info)
{
    trace("");
    trace("*** Started ***");

    gdt_init();
    idt_init();

    trace("    .text    %p - %p", TEXT_START, TEXT_END);
    trace("    .rodata  %p - %p", RODATA_START, RODATA_END);
    trace("    .data    %p - %p", DATA_START, DATA_END);
    trace("    .bss     %p - %p", BSS_START, BSS_END);

    const unsigned char* multiboot_end = multiboot_init(multiboot_info);
    trace("Multiboot end: %p", multiboot_end);

    kmalloc_init(multiboot_end + sizeof(uint32_t));
    test_kmalloc();

    multiboot_fix(multiboot_info);

    /* And 0xFFFFF000 points to initial_pagedir */
    trace("initial_pagedir: %p", initial_pagedir);
    trace("0xFFFFF000: %p", *((unsigned long*)0xFFFFF000));

    //trace("multiboot_info: 0x%X", multiboot_info);

    int mmap_count;
    const struct multiboot_mmap_entry* mmap = multiboot_get_mmap(&mmap_count);
    for(int i = 0; i < mmap_count; i++) {
        trace("mmap[%d]: 0x%llX-0x%llX 0x%llX 0x%X",
              i,
              mmap[i].addr,
              mmap[i].addr + mmap[i].len - 1,
              mmap[i].len,
              mmap[i].type);
    }

    test_bitset();

    pmm_init(mmap, mmap_count);
    test_pmm();

    /*
     * Mark all kernel memory as reserved
     */
    for(unsigned long page = (unsigned long)KERNEL_START - KERNEL_BASE;
        page < ALIGN((unsigned long)kmalloc_brk() - KERNEL_BASE, PAGE_SIZE);
        page += PAGE_SIZE) {

        pmm_reserve(page);
    }
    trace("Kernel break: %p", kmalloc_brk());

    /* TODO: Mark initial modules storage as free */

    vmm_init();
    trace("Kernel area: %p - %p", KERNEL_START, kmalloc_brk());

    /*
     * Now, remap each sections of kernel with appropriate permissions
     */
    for(unsigned char* page = TEXT_START; page < TEXT_END; page += PAGE_SIZE) {
        vmm_remap(page, 0);
    }
    for(unsigned char* page = RODATA_START; page < RODATA_END; page += PAGE_SIZE) {
        vmm_remap(page, 0);
    }

    /* This should throw a page fault */
    // ((char*)"aaa")[0] = '-';
    // *((unsigned char*)0xC0100000) = '-';
    // while(1);

    test_vmm();

    trace("Initializing pic");
    pic_init();

    trace("Initializing pit");
    pit_init();

    init_multitasking();
    struct task* task2 = task_create("task2", task2_entry);
    tasks[1] = task2;
    task1_entry();

#if 0
    size_t initrd_size;
    const void* initrd_data = multiboot_get_initrd(&initrd_size);
    if(!initrd_data) {
        panic("No initrd found");
    }

    trace("Loading initrd");
    initrd_init(initrd_data, initrd_size);

    trace("Loading hello.elf");
    const struct initrd_file* hello_elf = initrd_get_file("hello.elf");
    if(!hello_elf)
        panic("hello.elf not found");

    elf_entry_t hello_entry = load_elf(hello_elf->data, hello_elf->size);
    trace("hello_entry: %p", hello_entry);
    assert((unsigned char*)hello_entry == (unsigned char*)0x1000A0);

    unsigned char* userstack = kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
    trace("hello_stack: %p", userstack);
    vmm_remap(userstack, VMM_PAGE_USER|VMM_PAGE_WRITABLE);

    trace("Entering usermode");
    switch_to_usermode(userstack + PAGE_SIZE - sizeof(uint32_t), hello_entry);
    trace("Here????");
#endif

    trace("*** Rebooting ***");
    reboot();
}


