#include "mutex.h"
#include "assert.h"
#include "kernel.h"
#include "process.h"
#include <string.h>

void mutex_init(struct Mutex* mutex)
{
    memset(mutex, 0, sizeof(*mutex));
    STAILQ_INIT(&mutex->wait_queue);
}

static bool lock(struct Mutex* mutex, bool interruptible)
{
    enter_critical_section();
    if (mutex->owner == thread_current())
        panic("Attempt to lock a non-recursive mutex");

    while (mutex->owner != NULL &&
           (!interruptible || !thread_current()->process->exit_requested)) {
        queue_wait(&mutex->wait_queue, WAIT_INFINITE);
    }

    if (interruptible && thread_current()->process->exit_requested) {
        leave_critical_section();
        return false;
    }

    mutex->owner = thread_current();
    thread_current()->owned_mutexes++;
    leave_critical_section();
    return true;
}

bool mutex_lock(struct Mutex* mutex)
{
    return lock(mutex, true);
}

void mutex_lock_uninterruptible(struct Mutex* mutex)
{
    lock(mutex, false);
}

bool mutex_trylock(struct Mutex* mutex)
{
    enter_critical_section();
    if (thread_current()->process->exit_requested) {
        leave_critical_section();
        return false;
    } else if (mutex->owner == thread_current()) {
        leave_critical_section();
        return false;
    } else if (mutex->owner == NULL) {
        mutex->owner = thread_current();
        thread_current()->owned_mutexes++;
        leave_critical_section();
        return true;
    }
    leave_critical_section();
    return false;
}

void mutex_unlock(struct Mutex* mutex)
{
    enter_critical_section();
    if (!mutex->owner)
        panic("Attempt to unlock an unlocked mutex");
    else if (mutex->owner != thread_current())
        panic("Attempt by %s:%s to unlock a mutex owned by %s:%s",
              thread_current()->process->name, thread_current()->name,
              mutex->owner->process->name, mutex->owner->name);
    mutex->owner = NULL;
    thread_current()->owned_mutexes--;
    queue_notify(&mutex->wait_queue);
    leave_critical_section();
}

bool mutex_is_locked_by_current_thread(struct Mutex* mutex)
{
    enter_critical_section();
    bool result = mutex->owner && mutex->owner == thread_current();
    leave_critical_section();
    return result;
}

