#include "block_cache.h"
#include "assert.h"
#include "kmalloc.h"
#include "kthread.h"
#include "mutex.h"
#include "process.h"
#include <string.h>

#define CACHE_SIZE    128
#define BLOCK_SIZE    512
#define INVALID_INDEX -1

#define ENTRY_ASSIGNED   1 /* Cache entry is assigned */
#define ENTRY_VALID      2 /* BlockCacheEntry::data is valid */
#define ENTRY_REFERENCED 4 /* Referenced bit for CLOCK algorithm */
#define ENTRY_PINNED     8 /* I/O in progress for entry, do not evict */

struct Generation
{
    struct BlockDevice* dev;
    unsigned generation;
};

static struct BlockCacheEntry* cache;
static int hand;
static struct Generation* dev_generations;
static int dev_generation_count;
static struct WaitQueue availability_queue;
static struct Mutex metadata_lock;

static void move_hand(void)
{
    hand++;
    if (hand == CACHE_SIZE)
        hand = 0;
}

static bool unpin(struct BlockCacheEntry* entry, bool success)
{
    mutex_lock_uninterruptible(&metadata_lock);

    success = success && !thread_current()->process->exit_requested;
    if (success)
        entry->flags =
            (entry->flags | ENTRY_VALID | ENTRY_REFERENCED) & ~ENTRY_PINNED;
    else
        entry->flags = 0;

    queue_notify_all(&availability_queue);
    mutex_unlock(&metadata_lock);

    return success;
}

bool block_cache_init(void)
{
    cache = kmalloc(sizeof(struct BlockCacheEntry) * CACHE_SIZE);
    if (!cache)
        return false;
    memset(cache, 0, sizeof(struct BlockCacheEntry) * CACHE_SIZE);
    queue_init(&availability_queue);
    mutex_init(&metadata_lock);
    return true;
}

static void invalidate_dev(struct BlockDevice* dev)
{
    assert(mutex_is_locked_by_current_thread(&metadata_lock));
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].dev == dev)
            cache[i].flags &= ~ENTRY_ASSIGNED;
    }
}

/*
 * Lookup entry with the specified identity
 */
static int lookup_entry(struct BlockDevice* dev, uint64_t lba,
                        unsigned generation)
{
    assert(mutex_is_locked_by_current_thread(&metadata_lock));
    for (int i = 0; i < CACHE_SIZE; i++) {
        if ((cache[i].flags & ENTRY_ASSIGNED) && cache[i].dev == dev &&
            cache[i].generation == generation && cache[i].lba == lba) {
            return i;
        }
    }
    return INVALID_INDEX;
}

/*
 * Scan for a victim, clearing reference bits as needed
 * Publish the new identity and pins it before returning (while lock is held)
 * If all entries are pinned, return INVALID_INDEX
 */
static int clock_claim(struct BlockDevice* dev, uint64_t lba,
                       unsigned generation)
{
    assert(mutex_is_locked_by_current_thread(&metadata_lock));
    int result = INVALID_INDEX;
    int scanned = 0;
    int pinned = 0;
    while (result == INVALID_INDEX) {
        if (!(cache[hand].flags & ENTRY_PINNED)) {
            if (!(cache[hand].flags & ENTRY_ASSIGNED)) {
                cache[hand].flags =
                    ENTRY_ASSIGNED | ENTRY_REFERENCED | ENTRY_PINNED;
                cache[hand].dev = dev;
                cache[hand].lba = lba;
                cache[hand].generation = generation;
                result = hand;
            } else if (!(cache[hand].flags & ENTRY_REFERENCED)) {
                cache[hand].flags =
                    ENTRY_ASSIGNED | ENTRY_REFERENCED | ENTRY_PINNED;
                cache[hand].dev = dev;
                cache[hand].lba = lba;
                cache[hand].generation = generation;
                result = hand;
            } else {
                cache[hand].flags &= ~ENTRY_REFERENCED;
            }
        } else {
            pinned++;
        }
        move_hand();

        scanned++;
        if (result == INVALID_INDEX && scanned == CACHE_SIZE) {
            if (pinned == CACHE_SIZE) /* all entries were pinned */
                return INVALID_INDEX;
            scanned = pinned = 0;
        }
    }
    return result;
}

/*
 * Release metadata_lock then waits for availability
 * Reacquires metadata_lock after waking up
 */
static bool wait_for_availability()
{
    assert(mutex_is_locked_by_current_thread(&metadata_lock));

    enter_critical_section(); /* prevent lost notifications */
    mutex_unlock(&metadata_lock);
    queue_wait(&availability_queue, WAIT_INFINITE);
    leave_critical_section();

    return mutex_lock(&metadata_lock);
}

static bool entry_is_ready(struct BlockCacheEntry* entry)
{
    assert(mutex_is_locked_by_current_thread(&metadata_lock));
    bool result =
        (entry->flags & ENTRY_VALID) && !(entry->flags & ENTRY_PINNED);
    if (result)
        entry->flags |= ENTRY_PINNED;
    return result;
}

static void update_generation(struct BlockDevice* dev, unsigned generation)
{
    assert(mutex_is_locked_by_current_thread(&metadata_lock));
    int index = INVALID_INDEX;
    for (int i = 0; i < dev_generation_count; i++) {
        if (dev_generations[i].dev == dev) {
            index = i;
            break;
        }
    }

    if (index == INVALID_INDEX) {
        struct Generation* new_gen =
            kmalloc(sizeof(struct Generation) * (dev_generation_count + 1));
        if (!new_gen) {
            kfree(dev_generations);
            dev_generations = NULL;
            dev_generation_count = 0;
            invalidate_dev(dev);
            return;
        }

        if (dev_generations)
            memcpy(new_gen, dev_generations,
                   sizeof(struct Generation) * dev_generation_count);

        index = dev_generation_count++;

        kfree(dev_generations);
        dev_generations = new_gen;
        dev_generations[index].dev = dev;
        dev_generations[index].generation = generation;
    }

    if (dev_generations[index].generation != generation) {
        dev_generations[index].generation = generation;
        invalidate_dev(dev);
    }
}

bool block_cache_read(struct BlockDevice* dev, uint64_t lba, char* buffer,
                      size_t buffer_len)
{
    if (buffer_len != BLOCK_SIZE || !dev || !buffer)
        return false;

    unsigned generation;
    if (!dev->check_media(dev, &generation)) {
        if (!mutex_lock(&metadata_lock))
            return false;
        invalidate_dev(dev);
        mutex_unlock(&metadata_lock);
        return false;
    }

    if (!mutex_lock(&metadata_lock))
        return false;

    update_generation(dev, generation);

    int index = INVALID_INDEX;
    while (true) {
        index = lookup_entry(dev, lba, generation);
        if (index != INVALID_INDEX) {
            if (entry_is_ready(&cache[index]))
                break;
        } else {
            index = clock_claim(dev, lba, generation);
            if (index != INVALID_INDEX)
                break;
        }
        if (!wait_for_availability())
            return false;
    }

    assert(index != INVALID_INDEX);
    bool cached = cache[index].flags & ENTRY_VALID;
    mutex_unlock(&metadata_lock);

    if (cached) {
        memcpy(buffer, cache[index].data, BLOCK_SIZE);
        return unpin(&cache[index], true);
    }

    /* Now we can read entry at our own pace */
    bool success = dev->read(dev, lba, cache[index].data, BLOCK_SIZE);
    if (success)
        memcpy(buffer, cache[index].data, BLOCK_SIZE);
    return unpin(&cache[index], success);
}

bool block_cache_write(struct BlockDevice* dev, uint64_t lba,
                       const char* buffer, size_t buffer_len)
{
    if (buffer_len != BLOCK_SIZE || !dev || !buffer)
        return false;

    unsigned generation;
    if (!dev->check_media(dev, &generation)) {
        if (!mutex_lock(&metadata_lock))
            return false;
        invalidate_dev(dev);
        mutex_unlock(&metadata_lock);
        return false;
    }

    if (!mutex_lock(&metadata_lock))
        return false;

    update_generation(dev, generation);

    int index = INVALID_INDEX;
    while (true) {
        index = lookup_entry(dev, lba, generation);
        if (index != INVALID_INDEX) {
            if (entry_is_ready(&cache[index]))
                break;
        } else {
            index = clock_claim(dev, lba, generation);
            if (index != INVALID_INDEX)
                break;
        }
        if (!wait_for_availability())
            return false;
    }

    mutex_unlock(&metadata_lock);
    if (index == INVALID_INDEX)
        return false;

    bool result = dev->write(dev, lba, buffer, buffer_len);
    if (result)
        memcpy(&cache[index].data, buffer, BLOCK_SIZE);
    return unpin(&cache[index], result);
}

