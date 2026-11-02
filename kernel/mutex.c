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

bool mutex_lock(struct Mutex* mutex)
{
    enter_critical_section();
    if (mutex->owner == thread_current()) {
        leave_critical_section();
        return false;
    }

    while (mutex->owner != NULL &&
           !thread_current()->process->exit_requested) {
        queue_wait(&mutex->wait_queue, WAIT_INFINITE);
    }

    if (thread_current()->process->exit_requested) {
        leave_critical_section();
        return false;
    }

    mutex->owner = thread_current();
    thread_current()->owned_mutexes++;
    leave_critical_section();
    return true;
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
    assert(mutex->owner == thread_current());
    mutex->owner = NULL;
    thread_current()->owned_mutexes--;
    queue_notify(&mutex->wait_queue);
    leave_critical_section();
}

