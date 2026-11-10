#pragma once

#include "kthread.h"

/*
 * Non-recursive sleeping mutex
 */
struct Mutex
{
    struct Thread* owner;
    struct WaitQueue wait_queue;
};

void mutex_init(struct Mutex* mutex);
bool mutex_lock(struct Mutex* mutex);
void mutex_lock_uninterruptible(struct Mutex* mutex);
bool mutex_trylock(struct Mutex* mutex);
void mutex_unlock(struct Mutex* mutex);
bool mutex_is_locked_by_current_thread(struct Mutex* mutex);

