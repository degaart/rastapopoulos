#include "exec.h"
#include "kernel.h"
#include "kmalloc.h"
#include "pmm.h"
#include "process.h"
#include "psp.h"
#include "trace.h"
#include "vga.h"
#include "vmm.h"
#include <gdt.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct Usercode
{
    const char* name;
    size_t len;
    void* addr;
} __attribute__((packed));

extern struct Usercode usercode_data[];
extern uint8_t ring3_trampoline[];
extern uint8_t ring3_trampoline_end[];

#define RING3_TRAMPOLINE_BASE 0x1000
#define RING3_PSP_BASE        0x2000
#define USER_BASE             0x00400000
#define USER_STACK            (KERNEL_OFFSET - PAGE_SIZE)

struct ExecArgs
{
    char* filename;
    char* args;
    const struct Usercode* usercode;
};

static void destroy_exec_args(struct ExecArgs* exec_args)
{
    if (!exec_args)
        return;
    kfree(exec_args->args);
    kfree(exec_args->filename);
    kfree(exec_args);
}

static void user_entry(void* param)
{
    struct ExecArgs* args = param;
    if (!args || !args->filename || !args->args || !args->usercode) {
        destroy_exec_args(args);
        return;
    }
    if (strlen(args->args) > PAGE_SIZE - sizeof(struct ExecArgs) - 1) {
        trace("Arguments too large for PSP");
        destroy_exec_args(args);
        return;
    }

    /* Map ring3 trampoline */
    void* frame = pmm_alloc();
    if (!frame) {
        trace("Out of memory");
        destroy_exec_args(args);
        return;
    }
    if (!vmm_map(frame, (void*)RING3_TRAMPOLINE_BASE, VMM_USER | VMM_WRITABLE))
        panic("Failed to map page 0x%x", RING3_TRAMPOLINE_BASE);
    memcpy((void*)RING3_TRAMPOLINE_BASE, ring3_trampoline,
           ring3_trampoline_end - ring3_trampoline);
    if (!vmm_remap((void*)RING3_TRAMPOLINE_BASE, VMM_USER))
        panic("Failed to remap page 0x%x", RING3_TRAMPOLINE_BASE);

    /* Map Program Segment Prefix */
    frame = pmm_alloc();
    if (!frame)
        panic("Out of memory");
    if (!vmm_map(frame, (void*)RING3_PSP_BASE, VMM_USER | VMM_WRITABLE))
        panic("Failed to map page 0x%x", RING3_PSP_BASE);

    struct PSP* psp = (struct PSP*)RING3_PSP_BASE;
    psp->pid = process_current()->pid;
    psp->args = (char*)RING3_PSP_BASE + sizeof(struct PSP);
    psp->args_len = strlen(args->args);
    memcpy(psp->args, args->args, psp->args_len + 1);

    /* Map user code + data + bss */
    for (uintptr_t ptr = USER_BASE; ptr < USER_BASE + args->usercode->len;
         ptr += PAGE_SIZE) {
        frame = pmm_alloc();
        if (!frame)
            panic("Out of memory");
        if (!vmm_map(frame, (void*)ptr, VMM_USER | VMM_WRITABLE))
            panic("Failed to map page 0x%lx", ptr);
    }

    uint8_t* ptr = (void*)USER_BASE;
    memcpy(ptr, args->usercode->addr, args->usercode->len);

    /* Stack */
    frame = pmm_alloc();
    if (!frame)
        panic("Out of memory");

    if (!vmm_map(frame, (void*)USER_STACK, VMM_USER | VMM_WRITABLE))
        panic("Failed to map page");
    ptr = (void*)USER_STACK;
    memset(ptr, 0xcc, PAGE_SIZE);

    /* VGA mem */
    if (!vmm_map((void*)VGA_PHYSICAL_BASE, (void*)VGA_PHYSICAL_BASE,
                 VMM_USER | VMM_WRITABLE | VMM_SHARED))
        panic("Failed to map page 0x%x", VGA_PHYSICAL_BASE);

    destroy_exec_args(args);

    uint32_t esp = USER_STACK + PAGE_SIZE;
    ring3_jump((void*)RING3_TRAMPOLINE_BASE, (void*)(USER_STACK + PAGE_SIZE),
               USERCODESEG | 0x3, USERDATASEG | 0x3);
    panic("ring3_jump returned");
}

bool exec(const char* filename, const char* args)
{
    if (!filename)
        return false;
    if (!args)
        args = "";

    const struct Usercode* usercode = NULL;
    for (int i = 0; usercode_data[i].name; i++) {
        if (!strcmp(usercode_data[i].name, filename)) {
            usercode = &usercode_data[i];
            break;
        }
    }

    if (!usercode)
        return false;

    struct Process* process = process_create(filename);
    if (!process)
        return false;

    struct ExecArgs* exec_args = kmalloc(sizeof(struct ExecArgs));
    if (!exec_args)
        goto cleanup;
    memset(exec_args, 0, sizeof(*exec_args));

    exec_args->filename = strdup(filename);
    if (!exec_args->filename)
        goto cleanup;
    exec_args->args = strdup(args);
    if (!exec_args->args)
        goto cleanup;
    exec_args->usercode = usercode;

    /* When the child process exits, it's reference count is decreased
     * So, we need to explicitly retain it to avoid access a destroyed process
     */
    process_incref(process);
    if (!thread_create(process, "main", user_entry, exec_args)) {
        process_decref(process);
        goto cleanup;
    }

    event_wait(&process->termination_event);
    process_decref(process);
    return true;
cleanup:
    process->state = PROCESS_EXITED;
    process_deactivate(process);
    process_decref(process);
    destroy_exec_args(exec_args);
    return false;
}

