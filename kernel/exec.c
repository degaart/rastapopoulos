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

struct Context
{
    char* filename;
    char* args;
    const struct Usercode* usercode;
    bool started;
    int refcount;
};

static struct Context* context_create(const char* filename, const char* args,
                                      const struct Usercode* usercode)
{
    struct Context* result = kmalloc(sizeof(struct Context));
    if (!result)
        return NULL;
    memset(result, 0, sizeof(*result));
    result->filename = strdup(filename);
    if (!result->filename)
        goto cleanup;
    result->args = strdup(args);
    if (!result->args)
        goto cleanup;
    result->usercode = usercode;
    result->started = false;
    result->refcount = 1;
    return result;

cleanup:
    if (result->args)
        kfree(result->args);
    if (result->filename)
        kfree(result->filename);
    kfree(result);
    return NULL;
}

static int context_incref(struct Context* ctx)
{
    bool iflag = enter_critical_section();
    int result = ++ctx->refcount;
    leave_critical_section(iflag);
    return result;
}

static int context_decref(struct Context* ctx)
{
    if (!ctx)
        panic("context_decref with NULL ctx");

    bool iflag = enter_critical_section();
    if (ctx->refcount <= 0)
        panic("Calling exec_args_decref with refcount=%d", ctx->refcount);
    int result = --ctx->refcount;
    if (ctx->refcount > 0) {
        leave_critical_section(iflag);
        return result;
    }

    if (ctx->args)
        kfree(ctx->args);
    if (ctx->filename)
        kfree(ctx->filename);
    kfree(ctx);
    leave_critical_section(iflag);
    return result;
}

static void user_entry(void* param)
{
    struct Context* ctx = param;
    context_incref(ctx);

    if (!ctx || !ctx->filename || !ctx->args || !ctx->usercode) {
        context_decref(ctx);
        return;
    }
    if (strlen(ctx->args) > PAGE_SIZE - sizeof(struct Context) - 1) {
        trace("Arguments too large for PSP");
        context_decref(ctx);
        return;
    }

    /* Map ring3 trampoline */
    if (!vmm_alloc((void*)RING3_TRAMPOLINE_BASE, VMM_USER | VMM_WRITABLE)) {
        trace("Failed to map page 0x%x", RING3_TRAMPOLINE_BASE);
        context_decref(ctx);
        return;
    }
    memcpy((void*)RING3_TRAMPOLINE_BASE, ring3_trampoline,
           ring3_trampoline_end - ring3_trampoline);
    if (!vmm_remap((void*)RING3_TRAMPOLINE_BASE, VMM_USER)) {
        trace("Failed to remap page 0x%x", RING3_TRAMPOLINE_BASE);
        context_decref(ctx);
        return;
    }

    /* Map Program Segment Prefix */
    if (!vmm_alloc((void*)RING3_PSP_BASE, VMM_USER | VMM_WRITABLE)) {
        trace("Failed to map page 0x%x", RING3_PSP_BASE);
        context_decref(ctx);
        return;
    }

    struct PSP* psp = (struct PSP*)RING3_PSP_BASE;
    psp->pid = process_current()->pid;
    psp->args = (char*)RING3_PSP_BASE + sizeof(struct PSP);
    psp->args_len = strlen(ctx->args);
    memcpy(psp->args, ctx->args, psp->args_len + 1);

    /* Map user code + data + bss */
    for (uintptr_t ptr = USER_BASE; ptr < USER_BASE + ctx->usercode->len;
         ptr += PAGE_SIZE) {
        if (!vmm_alloc((void*)ptr, VMM_USER | VMM_WRITABLE)) {
            trace("Failed to map page 0x%lx", ptr);
            context_decref(ctx);
            return;
        }
    }

    uint8_t* ptr = (void*)USER_BASE;
    memcpy(ptr, ctx->usercode->addr, ctx->usercode->len);

    /* Stack */
    if (!vmm_alloc((void*)USER_STACK, VMM_USER | VMM_WRITABLE)) {
        trace("Failed to map page");
        context_decref(ctx);
        return;
    }
    ptr = (void*)USER_STACK;
    memset(ptr, 0xcc, PAGE_SIZE);

    /* VGA mem */
    if (!vmm_map((void*)VGA_PHYSICAL_BASE, (void*)VGA_PHYSICAL_BASE,
                 VMM_USER | VMM_WRITABLE | VMM_SHARED)) {
        trace("Failed to map page 0x%x", VGA_PHYSICAL_BASE);
        context_decref(ctx);
        return;
    }
    ctx->started = true;
    context_decref(ctx);

    uint32_t esp = USER_STACK + PAGE_SIZE;
    ring3_jump((void*)RING3_TRAMPOLINE_BASE, (void*)(USER_STACK + PAGE_SIZE),
               USERCODESEG | 0x3, USERDATASEG | 0x3);
    panic("ring3_jump returned");
}

int exec(const char* filename, const char* args)
{
    if (!filename)
        return -1;
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
        return -1;

    struct Process* process = process_create(filename);
    if (!process)
        return -1;

    struct Context* ctx = context_create(filename, args, usercode);
    if (!ctx)
        goto cleanup;

    /* When the child process exits, it's reference count is decreased
     * So, we need to explicitly retain it to avoid access a destroyed process
     */
    process_incref(process);
    if (!thread_create(process, "main", user_entry, ctx)) {
        process_decref(process);
        goto cleanup;
    }

    event_wait(&process->termination_event);

    int result = -1;
    if (ctx->started)
        result = process->exit_status;

    context_decref(ctx);
    process_decref(process);
    return result;
cleanup:
    process->state = PROCESS_EXITED;
    process_deactivate(process);
    if (ctx)
        context_decref(ctx);
    process_decref(process);
    return -1;
}

