#pragma once

/*
 * A recursive interrupt lock
 * Disables interrupts when lock() called
 * Then enabled them only if they were enabled before lock()
 * was called
 */
struct lock {
    int c;
    unsigned long v;
};

void lock_lock(struct lock*);
void lock_unlock(struct lock*);


