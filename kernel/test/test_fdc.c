#ifdef UNIT_TESTS
#    include "../cmos.h"
#    include "../fdc.h"
#    include "../kmalloc.h"
#    include "../kthread.h"
#    include "../pic.h"
#    include "../pit.h"
#    include "../process.h"
#    include "test.h"
#    include <string.h>

static char data[4 * 512];
static char expected[4 * 512];
static char original[512];
static char pattern[512];
static struct Event concurrent_done[2];

static void concurrent_reader(void* argument)
{
    unsigned index = (uintptr_t)argument;
    CHECK(fdc_read(0, index * 18, data + index * 512, 512));
    event_signal(&concurrent_done[index]);
}

static void test_secondary(void);
static void test_failure_recovery(void);

void test_fdc(void)
{
    trace("*** FDC tests started");
    unsigned attempts = fdc_test_attempt_count();
    CHECK(!fdc_read(-1, 0, data, 512));
    CHECK(!fdc_read(2, 0, data, 512));
    CHECK(!fdc_read(0, 0, data, 0));
    CHECK(!fdc_read(0, 0, data, 513));
    CHECK(!fdc_read(0, 0, NULL, 512));
    CHECK(!fdc_read(0, 0, (char*)0x1000, 512));
    CHECK(!fdc_read(0, UINT32_MAX, data, 512));
    CHECK(!fdc_read(0, 2879, data, 1024));
    CHECK(!fdc_write(2, 0, data, 512));
    CHECK(!fdc_write(0, 0, NULL, 512));
    CHECK(!fdc_write(0, 0, data, 0));
    CHECK(!fdc_write(0, 2879, data, 1024));
    CHECK(!fdc_read(0, 0, (char*)(UINTPTR_MAX - 255), 512));
    CHECK(!fdc_read(0, 0, (char*)"read-only", 512));
    CHECK(fdc_test_attempt_count() == attempts);
    CHECK(fdc_read(0, 0, data, 512));
    CHECK((unsigned char)data[510] == 0x55 &&
          (unsigned char)data[511] == 0xaa);
    for (unsigned i = 0; i < 4; i++)
        CHECK(fdc_read(0, 16 + i, expected + i * 512, 512));
    CHECK(fdc_read(0, 16, data, sizeof(data)));
    CHECK(memcmp(data, expected, sizeof(data)) == 0);
    for (unsigned i = 0; i < 4; i++)
        CHECK(fdc_read(0, 34 + i, expected + i * 512, 512));
    CHECK(fdc_read(0, 34, data, sizeof(data)));
    CHECK(memcmp(data, expected, sizeof(data)) == 0);
    for (unsigned i = 0; i < 2; i++)
        CHECK(thread_create(NULL, "fdc-test", concurrent_reader,
                            (void*)(uintptr_t)i));
    for (unsigned i = 0; i < 2; i++)
        CHECK(event_wait(&concurrent_done[i], 10000));
    CHECK(fdc_read(0, 2879, original, sizeof(original)));
    for (unsigned i = 0; i < sizeof(pattern); i++)
        pattern[i] = i ^ 0xa5;
    bool written = fdc_write(0, 2879, pattern, sizeof(pattern));
    bool read_back = fdc_read(0, 2879, data, 512);
    bool matched = memcmp(data, pattern, 512) == 0;
    bool restored = fdc_write(0, 2879, original, sizeof(original));
    CHECK(restored);
    CHECK(written && read_back && matched);
    CHECK(fdc_read(0, 2879, data, 512));
    CHECK(memcmp(data, original, 512) == 0);
    test_secondary();
    test_failure_recovery();
    trace("*** FDC tests done");
}

static unsigned secondary_type;
static char secondary_condition;

static void fw_select(uint16_t selector)
{
    outw(0x510, selector);
}

void test_fdc_prepare(void)
{
    fw_select(0);
    char signature[4];
    for (unsigned i = 0; i < 4; i++)
        signature[i] = inb(0x511);
    if (memcmp(signature, "QEMU", 4) != 0)
        return;
    fw_select(0x19);
    unsigned count = 0;
    for (unsigned i = 0; i < 4; i++)
        count = count * 256 + inb(0x511);
    if (count > 128)
        return;
    for (unsigned i = 0; i < count; i++) {
        uint8_t entry[64];
        for (unsigned j = 0; j < sizeof(entry); j++)
            entry[j] = inb(0x511);
        if (memcmp(entry + 8, "opt/rasta/fdc-test", 18) == 0) {
            fw_select(entry[4] * 256 + entry[5]);
            secondary_type = inb(0x511) - '0';
            secondary_condition = inb(0x511);
            CHECK(secondary_type <= 15);
            uint8_t types = cmos_read(CMOS_FLOPPY_DRIVES);
            cmos_write(CMOS_FLOPPY_DRIVES, (types & 0xf0) | secondary_type);
            return;
        }
    }
}

static void test_secondary(void)
{
    if (secondary_type == FDC_NONE || secondary_type > FDC_1440) {
        unsigned attempts = fdc_test_attempt_count();
        CHECK(!fdc_read(1, 0, data, 512));
        CHECK(!fdc_write(1, 0, data, 512));
        CHECK(fdc_test_attempt_count() == attempts);
        return;
    }
    if (secondary_condition == 'M') {
        unsigned attempts = fdc_test_attempt_count();
        CHECK(!fdc_read(1, 0, data, 512));
        CHECK(fdc_test_attempt_count() - attempts == 4);
        CHECK(fdc_read(0, 0, data, 512));
        trace("*** Missing media failure and recovery passed");
        return;
    }
    unsigned sectors[] = {0, 720, 2400, 1440, 2880};
    unsigned tracks[] = {0, 9, 15, 9, 18};
    unsigned capacity = sectors[secondary_type];
    CHECK(!fdc_read(1, capacity, data, 512));
    CHECK(!fdc_read(1, capacity - 1, data, 1024));
    unsigned positions[] = {0, tracks[secondary_type] - 1,
                            tracks[secondary_type] * 2 - 1, capacity - 4};
    for (unsigned i = 0; i < sizeof(positions) / sizeof(*positions); i++) {
        CHECK(fdc_read(1, positions[i], data, sizeof(data)));
        for (unsigned j = 0; j < 4; j++) {
            unsigned actual;
            memcpy(&actual, data + j * 512, sizeof(actual));
            CHECK(actual == positions[i] + j);
        }
    }
    CHECK(fdc_read(1, capacity - 1, original, sizeof(original)));
    unsigned attempts = fdc_test_attempt_count();
    bool written = fdc_write(1, capacity - 1, pattern, sizeof(pattern));
    unsigned write_attempts = fdc_test_attempt_count() - attempts;
    bool read_back = fdc_read(1, capacity - 1, data, 512);
    bool matched = memcmp(data, pattern, 512) == 0;
    if (secondary_condition == 'R') {
        CHECK(!written && read_back);
        CHECK(write_attempts == 4);
        CHECK(memcmp(data, original, 512) == 0);
        CHECK(fdc_read(0, 0, data, 512));
        trace("*** Write protection failure and recovery passed");
    } else {
        CHECK(fdc_write(1, capacity - 1, original, sizeof(original)));
        CHECK(written && read_back && matched);
    }
    if (secondary_condition != 'R') {
        for (unsigned i = 1; i < 3; i++) {
            CHECK(fdc_read(1, positions[i], expected, sizeof(expected)));
            for (unsigned j = 0; j < sizeof(data); j++)
                data[j] = j ^ 0x5a;
            bool written = fdc_write(1, positions[i], data, sizeof(data));
            bool read_back = fdc_read(1, positions[i], data, sizeof(data));
            bool matched = true;
            for (unsigned j = 0; j < sizeof(data); j++)
                matched &= (unsigned char)data[j] == ((j ^ 0x5a) & 0xff);
            bool restored =
                fdc_write(1, positions[i], expected, sizeof(expected));
            CHECK(restored && written && read_back && matched);
        }
    }
    trace("*** Drive 1 type %u (%u sectors) tests passed", secondary_type,
          capacity);
}

static struct Event canceled_started;
static struct Event canceled_gate;
static bool canceled_returned;

static void canceled_reader(void* unused)
{
    char* buffer = kmalloc(512 * 36);
    CHECK(buffer);
    event_signal(&canceled_started);
    canceled_returned = !fdc_read(0, 0, buffer, 512 * 36);
    kfree(buffer);
}

static void cancel_reader_process(void* unused)
{
    event_wait(&canceled_gate, WAIT_INFINITE);
    process_exit(0);
}

static void test_failure_recovery(void)
{
    if (secondary_type != FDC_NONE)
        return;
    struct Process* child = process_create("fdc-cancel");
    CHECK(child);
    process_incref(child);
    CHECK(thread_create(child, "reader", canceled_reader, NULL));
    CHECK(thread_create(child, "killer", cancel_reader_process, NULL));
    CHECK(event_wait(&canceled_started, 1000));
    thread_sleep_ms(20);
    event_signal(&canceled_gate);
    CHECK(event_wait(&child->termination_event, 1000));
    CHECK(canceled_returned);
    process_decref(child);
    char* guard = kmalloc(512 * 36);
    CHECK(guard);
    memset(guard, 0xcc, 512 * 36);
    CHECK(fdc_read(0, 0, data, 512));
    for (unsigned i = 0; i < 512 * 36; i++)
        CHECK((unsigned char)guard[i] == 0xcc);
    kfree(guard);
    unsigned attempts = fdc_test_attempt_count();
    uint64_t before = pit_get_ticks();
    pic_mask_irq(IRQ_FDC);
    bool failed = !fdc_read(0, 0, data, 512);
    pic_unmask_irq(IRQ_FDC);
    CHECK(failed);
    CHECK(fdc_test_attempt_count() - attempts == 4);
    CHECK(pit_ticks_to_ms(pit_get_ticks() - before) < 15000);
    CHECK(fdc_read(0, 0, data, 512));
    CHECK((unsigned char)data[510] == 0x55 &&
          (unsigned char)data[511] == 0xaa);
    trace("*** FDC cancellation, missing IRQ, retry exhaustion and recovery "
          "passed");
}

#endif
