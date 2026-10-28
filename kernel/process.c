#include "process.h"
#include "assert.h"
#include "bitset.h"
#include "event_queue.h"
#include "kmalloc.h"
#include "kthread.h"
#include "ps2kbd.h"
#include "string.h"
#include "trace.h"
#include "vga.h"

struct Process* kernel_task;
struct Process* current_proc;
struct Process* active_process;
static struct ProcessList processes = TAILQ_HEAD_INITIALIZER(processes);
static struct ProcessList exited_processes =
    TAILQ_HEAD_INITIALIZER(exited_processes);
static Bitset pid_bitset;

int allocate_pid(void)
{
    bool iflag = enter_critical_section();
    size_t pid = bitset_find(&pid_bitset);
    if (pid == SIZE_MAX) {
        leave_critical_section(iflag);
        return -1;
    }
    bitset_set(&pid_bitset, pid);
    leave_critical_section(iflag);
    return (int)pid;
}

void free_pid(uint8_t pid)
{
    assert(pid != 0);
    if (!bitset_unset(&pid_bitset, pid))
        panic("Failed to free pid %u", pid);
}

static struct Process* create_with_address_space(const char* name,
                                                 struct AddressSpace* as)
{
    struct Process* proc = kmalloc(sizeof(struct Process));
    if (!proc)
        return NULL;

    int pid = allocate_pid();
    if (pid < 0) {
        kfree(proc);
        return NULL;
    }

    memset(proc, 0, sizeof(*proc));
    proc->pid = pid;

    proc->name = strdup(name);
    if (!proc->name) {
        kfree(proc);
        free_pid(pid);
        return NULL;
    }

    proc->as = as;
    proc->state = PROCESS_CONSTRUCTING;
    STAILQ_INIT(&proc->threads);
    proc->live_threads = 0;
    proc->exit_status = 0;
    proc->exit_requested = false;
    proc->state = PROCESS_ALIVE;
    proc->parent = current_proc;
    if (proc->parent) { /* kernel_task should not have an input queue */
        proc->input_queue = kmalloc(sizeof(*proc->input_queue));
        if (!proc->input_queue) {
            kfree(proc->name);
            kfree(proc);
            free_pid(pid);
            return NULL;
        }
        KeyEventQueue_init(proc->input_queue);
    }
    proc->refcount = 1;

    bool iflag = enter_critical_section();
    TAILQ_INSERT_TAIL(&processes, proc, processes);
    leave_critical_section(iflag);

    active_process = proc;
    return proc;
}

void process_init(void)
{
    size_t pid_bitset_size = bitset_storage_size(UINT8_MAX);
    uint32_t* pid_bitset_storage = kmalloc(pid_bitset_size);
    if (!pid_bitset_storage)
        panic("Out of memory");
    bitset_init(&pid_bitset, pid_bitset_storage, UINT8_MAX);

    kernel_task =
        create_with_address_space("kernel_task", address_space_current());
    if (!kernel_task)
        panic("Failed to create kernel_task");

    kernel_task->state = PROCESS_ALIVE;
    current_proc = kernel_task;
}

struct Process* process_current(void)
{
    return current_proc;
}

struct Process* process_create(const char* name)
{
    struct AddressSpace* as = kmalloc(sizeof(struct AddressSpace));
    if (!as)
        return NULL;
    if (!address_space_create(as)) {
        kfree(as);
        return NULL;
    }

    struct Process* process = create_with_address_space(name, as);
    if (!process) {
        address_space_destroy(as);
        kfree(as);
        return NULL;
    }
    return process;
}

int process_refcount(struct Process* process)
{
    return process->refcount;
}

int process_incref(struct Process* process)
{
    bool iflag = enter_critical_section();
    int result = ++process->refcount;
    leave_critical_section(iflag);
    return result;
}

int process_decref(struct Process* proc)
{
    bool iflag = enter_critical_section();
    assert(proc->refcount > 0);
    proc->refcount--;
    if (proc->refcount) {
        int result = proc->refcount;
        leave_critical_section(iflag);
        return result;
    }
    leave_critical_section(iflag);

    trace("Destroying process %s", proc->name);

    assert(proc != kernel_task);
    assert(proc != current_proc);
    assert(proc->state == PROCESS_EXITED);
    assert(proc->live_threads == 0);
    assert(STAILQ_EMPTY(&proc->threads));

    iflag = enter_critical_section();
    TAILQ_REMOVE(&processes, proc, processes);

    struct Process* p;
    TAILQ_FOREACH(p, &processes, processes)
    {
        if (p->parent == proc)
            p->parent = proc->parent;
    }
    leave_critical_section(iflag);

    kfree(proc->input_queue);

    address_space_destroy(proc->as);
    kfree(proc->as);
    kfree(proc->name);
    free_pid(proc->pid);
    kfree(proc);
    return 0;
}

void process_add_thread(struct Process* proc, struct Thread* thread)
{
    assert(proc != NULL);
    assert(thread != NULL);
    assert(proc->state == PROCESS_ALIVE);

    if (thread->process)
        panic("Thread is already owned by process 0x%p", thread->process);
    STAILQ_INSERT_TAIL(&proc->threads, thread, processes);
    thread->process = proc;
    proc->live_threads++;
}

void process_remove_thread(struct Process* proc, struct Thread* thread)
{
    assert(proc != NULL);
    assert(thread != NULL);
    assert(thread->process == proc);

    bool iflag = enter_critical_section();
    STAILQ_REMOVE(&proc->threads, thread, Thread, processes);
    proc->live_threads--;

    if (proc->live_threads == 0) {
        vga_cleanup_process(proc);
        process_deactivate(proc);
        proc->state = PROCESS_EXITED;
        event_signal(&proc->termination_event);
        TAILQ_INSERT_TAIL(&exited_processes, proc, exited_processes);
    }
    leave_critical_section(iflag);
}

void process_set_current(struct Process* proc)
{
    assert(proc);

    bool iflag = enter_critical_section();
    current_proc = proc;
    address_space_activate(proc->as);
    leave_critical_section(iflag);
}

void process_exit(uint8_t status)
{
    struct Process* proc = process_current();
    assert(proc != kernel_task);

    process_deactivate(current_proc);

    proc->exit_status = status;
    proc->exit_requested = true;
    proc->state = PROCESS_EXITING;

    /* Wake event waiters */
    bool iflag = enter_critical_section();
    struct Thread* thread;
    STAILQ_FOREACH(thread, &proc->threads, processes)
    {
        thread_cancel_wait(thread);
    }
    leave_critical_section(iflag);
    thread_exit();
    __builtin_unreachable();
}

void process_cleanup_exited(void)
{
    bool iflag = enter_critical_section();

    struct Process* proc;
    struct Process* tmp;
    TAILQ_FOREACH_SAFE(proc, &exited_processes, exited_processes, tmp)
    {
        TAILQ_REMOVE(&exited_processes, proc, exited_processes);
        process_decref(proc);
    }
    leave_critical_section(iflag);
}

void process_push_input_event(const struct KeyEvent* event)
{
    bool iflag = enter_critical_section();
    if (!active_process->input_queue) {
        leave_critical_section(iflag);
        return;
    }
    if (!KeyEventQueue_push(active_process->input_queue, event)) {
        active_process->input_queue_overflow = true;
        trace("Input queue overflow");
    }
    event_signal(&active_process->input_event);
    leave_critical_section(iflag);
}

static bool get_input_event(struct KeyEvent* event, bool* overflow, bool block)
{
    bool iflag = enter_critical_section();
    if (!current_proc->input_queue) {
        leave_critical_section(iflag);
        return false;
    }

    while (block && KeyEventQueue_empty(current_proc->input_queue) &&
           !current_proc->input_queue_overflow &&
           !current_proc->exit_requested) {
        event_wait(&current_proc->input_event, WAIT_INFINITE);
    }

    if (current_proc->exit_requested) {
        leave_critical_section(iflag);
        thread_exit();
        __builtin_unreachable();
    } else if (KeyEventQueue_empty(current_proc->input_queue) &&
               current_proc->input_queue_overflow) {
        *overflow = true;
        current_proc->input_queue_overflow = false;
        leave_critical_section(iflag);
        return true;
    }

    *overflow = false;
    bool result = KeyEventQueue_pop(current_proc->input_queue, event);
    leave_critical_section(iflag);
    return result;
}

bool process_get_input_event(struct KeyEvent* event, bool* overflow)
{
    return get_input_event(event, overflow, true);
}

bool process_peek_input_event(struct KeyEvent* event, bool* overflow)
{
    return get_input_event(event, overflow, false);
}

void process_deactivate(struct Process* process)
{
    bool iflag = enter_critical_section();
    struct Process* ancestor = process->parent;
    while (ancestor && ancestor->parent) {
        if (ancestor->input_queue) {
            active_process = ancestor;
            leave_critical_section(iflag);
            return;
        }
        ancestor = ancestor->parent;
    }
    active_process = kernel_task;
    leave_critical_section(iflag);
}

void process_daemonize(struct Process* process)
{
    bool iflag = enter_critical_section();
    kfree(process->input_queue);
    process->input_queue = NULL;
    process_deactivate(process);
    leave_critical_section(iflag);
}

void process_get_info(char* buffer, size_t len)
{
    if (len < 2)
        return;

    bool iflag = enter_critical_section();
    snprintf(buffer, len, "CP: %s AP: %s",
             current_proc ? current_proc->name : "NULL",
             active_process ? active_process->name : "NULL");
    leave_critical_section(iflag);
}

