#include "../block_cache.h"
#include "../fdc.h"
#include "../kthread.h"
#include "../pit.h"
#include "test.h"
#include <string.h>

#define DRIVE           0
#define SCRATCH_SECTOR1 2876
#define SCRATCH_SECTOR2 2877
#define SCRATCH_SECTOR3 2878
#define SCRATCH_SECTOR4 2879

static char buffer[512];

extern void block_cache_dump(void);

static struct Event concurrent_start;
static struct Event concurrent_done[3];

static void concurrent_access(void* argument)
{
    unsigned index = (uintptr_t)argument;
    char data[512];
    CHECK(event_wait(&concurrent_start, WAIT_INFINITE));
    for (unsigned iteration = 0; iteration < 3; iteration++) {
        memset(data, 0x30 + index, sizeof(data));
        CHECK(block_cache_write(DRIVE, SCRATCH_SECTOR1, data, sizeof(data)));
        CHECK(block_cache_read(DRIVE, SCRATCH_SECTOR1, data, sizeof(data)));
        CHECK(data[0] >= 0x30 && data[0] <= 0x32);
        for (unsigned i = 1; i < sizeof(data); i++)
            CHECK(data[i] == data[0]);
    }
    event_signal(&concurrent_done[index]);
}

void test_block_cache(void)
{
    trace("*** Block cache tests started");

    uint64_t start = pit_get_ticks();
    CHECK(block_cache_read(DRIVE, SCRATCH_SECTOR1, buffer, sizeof(buffer)));
    uint64_t elapsed = pit_get_ticks() - start;
    trace("First read: %llu ticks", elapsed);

    start = pit_get_ticks();
    CHECK(block_cache_read(DRIVE, SCRATCH_SECTOR1, buffer, sizeof(buffer)));
    elapsed = pit_get_ticks() - start;
    trace("Second read: %llu ticks", elapsed);

    /* If we write to the same sector again, it will be cached and the
     * subsequent read will be fast
     */
    memset(buffer, 0xcc, sizeof(buffer));
    CHECK(block_cache_write(DRIVE, SCRATCH_SECTOR1, buffer, sizeof(buffer)));

    start = pit_get_ticks();
    CHECK(block_cache_read(DRIVE, SCRATCH_SECTOR1, buffer, sizeof(buffer)));
    elapsed = pit_get_ticks() - start;
    trace("Third read: %llu ticks", elapsed);

    /* Now if we write to two more sectors, SCRATCH_SECTOR1 will be
     * invalidated
     */
    CHECK(block_cache_write(DRIVE, SCRATCH_SECTOR2, buffer, sizeof(buffer)));
    trace("Write sector %u", SCRATCH_SECTOR2);

    CHECK(block_cache_write(DRIVE, SCRATCH_SECTOR3, buffer, sizeof(buffer)));
    trace("Write sector %u", SCRATCH_SECTOR3);

    start = pit_get_ticks();
    CHECK(block_cache_read(DRIVE, SCRATCH_SECTOR1, buffer, sizeof(buffer)));
    elapsed = pit_get_ticks() - start;
    trace("Fourth read: %llu ticks", elapsed);

    event_init(&concurrent_start);
    for (unsigned i = 0; i < 3; i++) {
        event_init(&concurrent_done[i]);
        CHECK(thread_create(NULL, "cache-access", concurrent_access,
                            (void*)(uintptr_t)i));
    }
    for (unsigned i = 0; i < 3; i++)
        event_signal(&concurrent_start);
    for (unsigned i = 0; i < 3; i++) {
        CHECK(event_wait(&concurrent_done[i], 20000));
        event_destroy(&concurrent_done[i]);
    }
    event_destroy(&concurrent_start);
    char disk_data[512];
    CHECK(fdc_read(DRIVE, SCRATCH_SECTOR1, disk_data, sizeof(disk_data)));
    CHECK(block_cache_read(DRIVE, SCRATCH_SECTOR1, buffer, sizeof(buffer)));
    CHECK(memcmp(buffer, disk_data, sizeof(buffer)) == 0);
    CHECK(block_cache_read(DRIVE, SCRATCH_SECTOR2, buffer, sizeof(buffer)));
    CHECK(block_cache_read(DRIVE, SCRATCH_SECTOR3, buffer, sizeof(buffer)));
    trace("*** Block cache tests done");
}

