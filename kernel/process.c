#include "process.h"
#include "assert.h"
#include "bitset.h"
#include "kmalloc.h"
#include "kthread.h"
#include "string.h"
#include "trace.h"

struct Process* kernel_task;
struct Process* current_proc;
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
    proc->exit_status = 255;
    proc->exit_requested = false;

    bool iflag = enter_critical_section();
    TAILQ_INSERT_TAIL(&processes, proc, processes);
    leave_critical_section(iflag);

    proc->state = PROCESS_ALIVE;
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

void process_destroy(struct Process* proc)
{
    trace("Destroying process %s", proc->name);

    assert(proc != kernel_task);
    assert(proc != current_proc);
    assert(proc->state == PROCESS_EXITED);
    assert(proc->live_threads == 0);
    assert(STAILQ_EMPTY(&proc->threads));

    bool iflag = enter_critical_section();
    TAILQ_REMOVE(&processes, proc, processes);
    leave_critical_section(iflag);

    address_space_destroy(proc->as);
    kfree(proc->as);
    kfree(proc->name);
    free_pid(proc->pid);
    kfree(proc);
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
        proc->state = PROCESS_EXITED;
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

    proc->exit_status = status;
    proc->exit_requested = true;
    proc->state = PROCESS_EXITING;

    /* Wake event waiters */
    bool iflag = enter_critical_section();
    struct Thread* thread;
    STAILQ_FOREACH(thread, &proc->threads, processes)
    {
        struct Event* event = thread->waiting_event;
        if (event && thread->state == THREAD_BLOCKED) {
            event->waiter = NULL;
            thread->waiting_event = NULL;
            scheduler_enqueue(thread);
        }
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
        process_destroy(proc);
    }
    TAILQ_INIT(&exited_processes);
    leave_critical_section(iflag);
}

