#include "../kernel.h"
#include "../tar.h"
#include "../trace.h"
#include <stdint.h>
#include <string.h>

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            trace("Assert failed: %s", #cond);                                \
            debugexit(2);                                                     \
        }                                                                     \
    } while (0)

extern const uint8_t ramdisk_start[];
extern const uint8_t ramdisk_end[];

#define MAX_ENTRIES 64
struct TarEntryList
{
    struct TarEntry entries[MAX_ENTRIES];
    size_t len;
};

static void list_callback(const struct TarEntry* entry, void* ctx)
{
    struct TarEntryList* entries = ctx;
    CHECK(entries->len < MAX_ENTRIES);

    memcpy(&entries->entries[entries->len], entry, sizeof(*entry));
    entries->len++;
}

void test_tar(void)
{
    struct Tar tar;

    CHECK(tar_open(&tar, ramdisk_start, ramdisk_end - ramdisk_start) != false);

    struct TarEntryList list_buffer = {};
    CHECK(tar_list(&tar, list_callback, &list_buffer) != false);
    CHECK(list_buffer.len == 5);

    CHECK(!strcmp(list_buffer.entries[0].name, ""));
    CHECK(list_buffer.entries[0].is_dir);
    CHECK(list_buffer.entries[0].size == 0);

    CHECK(!strcmp(list_buffer.entries[1].name, "message"));
    CHECK(!list_buffer.entries[1].is_dir);
    CHECK(list_buffer.entries[1].size == 31);

    CHECK(!strcmp(list_buffer.entries[2].name, "fib.bin"));
    CHECK(!list_buffer.entries[2].is_dir);
    CHECK(list_buffer.entries[2].size == 436);

    CHECK(!strcmp(list_buffer.entries[3].name, "shell.bin"));
    CHECK(!list_buffer.entries[3].is_dir);
    CHECK(list_buffer.entries[3].size == 12984);

    CHECK(!strcmp(list_buffer.entries[4].name, "snek.bin"));
    CHECK(!list_buffer.entries[4].is_dir);
    CHECK(list_buffer.entries[4].size == 17732);

    void* const message_buffer;
    size_t message_buffer_len;
    CHECK(
        tar_open_file(&tar, "message", &message_buffer, &message_buffer_len));

    const char* expected = "ALL YOUR BASE ARE BELONG TO US\n";
    CHECK(message_buffer != NULL);
    CHECK(message_buffer_len == strlen(expected));
    CHECK(!memcmp(message_buffer, expected, strlen(expected)));

    trace("All tests OK");
}

