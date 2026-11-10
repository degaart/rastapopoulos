#include "../block_cache.h"
#include "../kthread.h"
#include "../pit.h"
#include "../process.h"
#include "test.h"
#include <string.h>

#ifndef BLOCK_CACHE_NULL_CASE
#    define BLOCK_CACHE_NULL_CASE 0
#endif

#define SECTORS 160
static struct FakeDevice
{
    struct BlockDevice device;
    unsigned generation, reads, writes;
    bool available, fail_read, fail_write, hold, succeed_after_cancel;
    struct Event entered, release;
    char sectors[SECTORS][512];
} devices[2];

static bool media(struct BlockDevice* device, unsigned* generation)
{
    struct FakeDevice* fake = (struct FakeDevice*)device;
    *generation = fake->generation;
    return fake->available;
}

static bool transfer(struct FakeDevice* fake)
{
    enter_critical_section();
    bool hold = fake->hold;
    fake->hold = false;
    leave_critical_section();
    if (hold) {
        event_signal(&fake->entered);
        bool result = event_wait(&fake->release, 2000);
        return result || fake->succeed_after_cancel;
    }
    return true;
}

static bool read_sector(struct BlockDevice* device, uint64_t lba, char* data,
                        size_t size)
{
    struct FakeDevice* fake = (struct FakeDevice*)device;
    enter_critical_section();
    fake->reads++;
    leave_critical_section();
    if (!transfer(fake) || fake->fail_read || lba >= SECTORS || size != 512) {
        fake->fail_read = false;
        return false;
    }
    memcpy(data, fake->sectors[lba], size);
    return true;
}

static bool write_sector(struct BlockDevice* device, uint64_t lba,
                         const char* data, size_t size)
{
    struct FakeDevice* fake = (struct FakeDevice*)device;
    enter_critical_section();
    fake->writes++;
    leave_critical_section();
    if (!transfer(fake) || fake->fail_write || lba >= SECTORS || size != 512) {
        fake->fail_write = false;
        return false;
    }
    memcpy(fake->sectors[lba], data, size);
    return true;
}

static void verify(char* data, unsigned value)
{
    for (unsigned i = 0; i < 512; i++)
        CHECK((unsigned char)data[i] == (unsigned char)value);
}

static struct Worker
{
    struct Event done;
    bool write;
    unsigned lba, iterations;
    char data[512];
} workers[4];

static void access_sector(void* argument)
{
    struct Worker* worker = argument;
    unsigned iterations = worker->iterations ? worker->iterations : 1;
    for (unsigned iteration = 0; iteration < iterations; iteration++) {
        if (worker->write)
            CHECK(block_cache_write(&devices[0].device, worker->lba,
                                    worker->data, 512));
        else {
            CHECK(block_cache_read(&devices[0].device, worker->lba,
                                   worker->data, 512));
            verify(worker->data, (unsigned char)worker->data[0]);
        }
        thread_yield();
    }
    event_signal(&worker->done);
}

static struct Event cancel_gate;
static bool canceled_result, cancel_write;

static void canceled_access(void* unused)
{
    char data[512];
    memset(data, 59, sizeof(data));
    canceled_result = cancel_write
                          ? block_cache_write(&devices[0].device, 0, data, 512)
                          : block_cache_read(&devices[0].device, 0, data, 512);
}

static void cancel_owner(void* unused)
{
    CHECK(event_wait(&cancel_gate, 2000));
    process_exit(0);
}

static void wait_until_blocked(struct Thread* thread)
{
    uint64_t start = pit_get_ticks();
    for (;;) {
        enter_critical_section();
        bool blocked =
            thread->state == THREAD_BLOCKED && thread->waiting_queue;
        leave_critical_section();
        if (blocked)
            return;
        CHECK(pit_get_ticks() - start < pit_ms_to_ticks(2000));
        thread_sleep_ms(1);
    }
}

static void cancellation_cases(void)
{
    struct FakeDevice* fake = &devices[0];
    char data[512];
    for (unsigned mode = 0; mode < 6; mode++) {
        trace("Cache cancellation case %u", mode);
        fake->generation++;
        fake->hold = true;
        fake->succeed_after_cancel = mode >= 4;
        canceled_result = true;
        cancel_write = mode & 1;
        bool waiting = mode < 2;
        if (waiting) {
            event_init(&workers[0].done);
            workers[0].write = false;
            CHECK(thread_create(NULL, "cache-holder", access_sector,
                                &workers[0]));
            CHECK(event_wait(&fake->entered, 2000));
        }
        unsigned reads = fake->reads;
        unsigned writes = fake->writes;
        event_init(&cancel_gate);
        struct Process* child = process_create("cache-cancel");
        CHECK(child);
        process_incref(child);
        struct Thread* thread =
            thread_create(child, "cache-canceled", canceled_access, NULL);
        CHECK(thread);
        CHECK(thread_create(child, "cache-killer", cancel_owner, NULL));
        if (waiting)
            wait_until_blocked(thread);
        else
            CHECK(event_wait(&fake->entered, 2000));
        event_signal(&cancel_gate);
        CHECK(event_wait(&child->termination_event, 2000));
        CHECK(!canceled_result);
        process_decref(child);
        event_destroy(&cancel_gate);
        if (waiting) {
            CHECK(fake->reads == reads && fake->writes == writes);
            event_signal(&fake->release);
            CHECK(event_wait(&workers[0].done, 2000));
            event_destroy(&workers[0].done);
        }
        CHECK(block_cache_read(&fake->device, 0, data, 512));
        CHECK(!memcmp(data, fake->sectors[0], 512));
    }
    fake->succeed_after_cancel = false;
}

void test_block_cache(void)
{
    trace("*** Block cache tests started");
    for (unsigned d = 0; d < 2; d++) {
        struct FakeDevice* fake = &devices[d];
        fake->device.check_media = media;
        fake->device.read = read_sector;
        fake->device.write = write_sector;
        fake->generation++;
        fake->available = true;
        event_init(&fake->entered);
        event_init(&fake->release);
        for (unsigned lba = 0; lba < SECTORS; lba++)
            memset(fake->sectors[lba], lba + d, 512);
    }
    struct FakeDevice* fake = &devices[0];
    struct BlockDevice* device = &fake->device;
    char data[512];
#if BLOCK_CACHE_NULL_CASE
    trace("Cache null-buffer case %u", BLOCK_CACHE_NULL_CASE);
    if (BLOCK_CACHE_NULL_CASE > 2)
        CHECK(block_cache_read(device, 0, data, 512));
    unsigned reads = fake->reads;
    unsigned writes = fake->writes;
    if (BLOCK_CACHE_NULL_CASE & 1)
        CHECK(!block_cache_read(device, 0, NULL, 512));
    else
        CHECK(!block_cache_write(device, 0, NULL, 512));
    CHECK(fake->reads == reads && fake->writes == writes);
#endif
    trace("Cache argument validation and basic I/O");
    CHECK(!block_cache_read(NULL, 0, data, 512));
    CHECK(!block_cache_write(NULL, 0, data, 512));
    CHECK(!block_cache_read(device, 0, data, 511));
    CHECK(!block_cache_write(device, 0, data, 513));
    fake->generation++;
    unsigned before = fake->reads;
    CHECK(block_cache_read(device, 0, data, 512));
    verify(data, 0);
    memset(data, 99, 512);
    CHECK(block_cache_read(device, 0, data, 512));
    verify(data, 0);
    CHECK(fake->reads == before + 1);
    CHECK(block_cache_read(&devices[1].device, 0, data, 512));
    verify(data, 1);
    memset(data, 77, 512);
    CHECK(block_cache_write(device, 0, data, 512));
    verify(fake->sectors[0], 77);
    memset(data, 0, 512);
    CHECK(block_cache_read(device, 0, data, 512));
    verify(data, 77);
    before = fake->reads;
    unsigned writes_before = fake->writes;
    for (unsigned i = 0; i < sizeof(data); i++)
        data[i] = (char)(i * 17 + 3);
    CHECK(block_cache_write(device, 9, data, sizeof(data)));
    CHECK(!memcmp(data, fake->sectors[9], sizeof(data)));
    memset(data, 0, sizeof(data));
    CHECK(block_cache_read(device, 9, data, sizeof(data)));
    for (unsigned i = 0; i < sizeof(data); i++)
        CHECK((unsigned char)data[i] == (unsigned char)(i * 17 + 3));
    CHECK(fake->reads == before);
    CHECK(fake->writes == writes_before + 1);
    memset(fake->sectors[9], 9, 512);
    fake->generation++;
    trace("Cache failure recovery and media invalidation");
    memset(data, 66, 512);
    fake->fail_write = true;
    CHECK(!block_cache_write(device, 0, data, 512));
    CHECK(block_cache_read(device, 0, data, 512));
    verify(data, 77);
    fake->generation++;
    fake->fail_read = true;
    CHECK(!block_cache_read(device, 0, data, 512));
    CHECK(block_cache_read(device, 0, data, 512));
    fake->available = false;
    CHECK(!block_cache_read(device, 0, data, 512));
    CHECK(!block_cache_write(device, 0, data, 512));
    fake->available = true;
    memset(fake->sectors[0], 88, 512);
    CHECK(block_cache_read(device, 0, data, 512));
    verify(data, 88);
    fake->generation++;
    trace("Cache replacement");
    for (unsigned lba = 0; lba < SECTORS; lba++) {
        CHECK(block_cache_read(device, lba, data, 512));
        verify(data, lba ? lba : 88);
    }
    before = fake->reads;
    CHECK(block_cache_read(device, 0, data, 512));
    CHECK(fake->reads == before + 1);
    trace("Cache held I/O contention");
    for (unsigned round = 0; round < 16; round++) {
        fake->generation++;
        fake->hold = true;
        before = fake->reads;
        for (unsigned i = 0; i < 4; i++) {
            event_init(&workers[i].done);
            workers[i].write = round % 3 == 1 || (round % 3 == 2 && i > 0);
            memset(workers[i].data, 100 + i, 512);
            struct Thread* thread = thread_create(NULL, "cache-worker",
                                                  access_sector, &workers[i]);
            CHECK(thread);
            if (i)
                wait_until_blocked(thread);
            if (!i)
                CHECK(event_wait(&fake->entered, 2000));
        }
        CHECK(block_cache_read(device, 1, data, 512));
        event_signal(&fake->release);
        for (unsigned i = 0; i < 4; i++) {
            CHECK(event_wait(&workers[i].done, 2000));
            event_destroy(&workers[i].done);
        }
        if (!(round % 3))
            CHECK(fake->reads == before + 2);
        CHECK(block_cache_read(device, 0, data, 512));
        CHECK(!memcmp(data, fake->sectors[0], 512));
    }
    trace("Cache concurrent read/write stress");
    for (unsigned pass = 0; pass < 2; pass++) {
        for (unsigned i = 0; i < 4; i++) {
            event_init(&workers[i].done);
            workers[i].iterations = 64;
            workers[i].lba = pass ? i : 0;
            workers[i].write = i & 1;
            memset(workers[i].data, 110 + i, 512);
            CHECK(thread_create(NULL, "cache-stress", access_sector,
                                &workers[i]));
        }
        for (unsigned i = 0; i < 4; i++) {
            CHECK(event_wait(&workers[i].done, 5000));
            event_destroy(&workers[i].done);
        }
        for (unsigned lba = 0; lba < 4; lba++) {
            CHECK(block_cache_read(device, lba, data, 512));
            CHECK(!memcmp(data, fake->sectors[lba], 512));
        }
    }
    for (unsigned i = 0; i < 4; i++) {
        workers[i].iterations = 1;
        workers[i].lba = 0;
    }
    cancellation_cases();
    trace("*** Block cache tests done");
}
