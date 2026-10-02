#include "rastaldr_glue.h"
#include <allocator.h>
#include <assert.h>
#include <crc32.h>
#include <fat12.h>
#include <multiboot.h>
#include <rastaldr.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILENAME "KERNEL.ELF"

static void test_open(const struct BPB* bpb, const void* fat,
                      const void* host_file, uint32_t host_file_size)
{
    printf("%s\n", __FUNCTION__);

    struct File f;
    if (!fat12_open(bpb, fat, &f, FILENAME)) {
        panic("fat12_open failed");
    }
    assert(f.size == host_file_size);
    fat12_close(&f);
}

static void test_complete_read(const struct BPB* bpb, const void* fat,
                               const uint8_t* host_file,
                               uint32_t host_file_size)
{
    printf("%s\n", __FUNCTION__);

    struct File f;
    if (!fat12_open(bpb, fat, &f, FILENAME)) {
        panic("fat12_open failed");
    }

    uint32_t offset = 0;
    uint8_t buffer[512];
    while (true) {
        if (offset == 2048)
            printf("Here\n");
        int nread = fat12_read(&f, buffer, sizeof(buffer));
        if (nread == -1) {
            panic("I/O error");
        } else if (nread == 0) {
            break;
        }

        for (int i = 0; i < nread; i++) {
            if (buffer[i] != host_file[i]) {
                panic("Comparison failed at offset %u: expected 0x%02x, got "
                      "0x%02x",
                      offset + i, host_file[i], buffer[i]);
            }
        }

        offset += nread;
        host_file += nread;
        assert(offset <= f.size);
    }

    assert(offset == f.size);
    fat12_close(&f);
}

static void test_offset_read(const struct BPB* bpb, const void* fat,
                             const void* host_file, uint32_t host_file_size)
{
    printf("%s\n", __FUNCTION__);

    struct File f;
    if (!fat12_open(bpb, fat, &f, FILENAME)) {
        panic("fat12_open failed");
    }

    uint8_t buffer[512];
    assert(fat12_seek(&f, 8) == 8);
    assert(fat12_read(&f, buffer, 4) == 4);

    for (int i = 0; i < 4; i++) {
        assert(buffer[i] == ((uint8_t*)host_file)[i + 8]);
    }

    assert(fat12_seek(&f, f.size) == f.size);

    assert(fat12_seek(&f, f.size - 4) == f.size - 4)
        assert(fat12_read(&f, buffer, 4) == 4);
    assert(*((uint32_t*)buffer) == *((uint32_t*)(host_file + f.size - 4)));

    assert(fat12_seek(&f, f.size + 1) == f.size);
    assert(fat12_seek(&f, f.size + 1024) == f.size);

    fat12_close(&f);
}

static void test_truncated_read(const struct BPB* bpb, const void* fat,
                                const void* host_file, uint32_t host_file_size)
{
    printf("%s\n", __FUNCTION__);

    struct File f;
    if (!fat12_open(bpb, fat, &f, FILENAME)) {
        panic("fat12_open failed");
    }

    assert(fat12_seek(&f, f.size - 16) == f.size - 16);

    uint8_t buffer[512];
    assert(fat12_read(&f, buffer, sizeof(buffer)) == 16);
    assert(!memcmp(buffer, host_file + f.size - 16, 16));
    assert(f.offset == f.size);
    assert(fat12_read(&f, buffer, sizeof(buffer)) == 0);

    fat12_close(&f);
}

static void test_large_read(const struct BPB* bpb, const void* fat,
                            const uint8_t* host_file, uint32_t host_file_size)
{
    printf("%s\n", __FUNCTION__);

    struct File f;
    if (!fat12_open(bpb, fat, &f, FILENAME)) {
        panic("fat12_open failed");
    }

    assert(fat12_seek(&f, 1) == 1);

    uint32_t read_size = (host_file_size > 8192 ? 8192 : host_file_size) - 1;
    uint8_t* buffer = malloc(read_size);
    assert(fat12_read(&f, buffer, read_size) == read_size);

    for (int i = 0; i < read_size; i++) {
        if (buffer[i] != host_file[i + 1]) {
            panic(
                "Comparison failed at offset %u: expected 0x%02x, got 0x%02x",
                i, host_file[i], buffer[i]);
        }
    }

    assert(f.offset == f.size);
    assert(fat12_read(&f, buffer, 1) == 0);

    free(buffer);
    fat12_close(&f);
}

int main()
{
    /*
     * Testcase1: Reading the entirety of KERNEL.ELF should yield the same
     *            content as reading ../bootloader/build/kernel.stripped.elf
     * Testcase2: Reading 16 bytes from offset 512 of KERNEL.ELF should yield
     *            the same content as ../bootloader/build/kernel.stripped.elf
     * Testcase3: Reading the last 16 bytes of KERNEL.ELF should yield the same
     *            content as ../bootloader/build/kernel.stripped.elf
     */

    /* Init 32K heap */
    static const int HEAP_SIZE = 32768;
    void* heap = mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!heap) {
        panic("mmap failed");
    }

    heap_init(heap, HEAP_SIZE);
    size_t heap_free = heap_info();

    /* Mmap the entirety of ../bootloader/build/kernel.stripped.elf */
    int host_file_fd =
        open("../bootloader/build/kernel.stripped.elf", O_RDONLY);
    if (host_file_fd == -1) {
        panic("open failed");
    }

    struct stat st;
    if (fstat(host_file_fd, &st) == -1) {
        panic("fstat failed");
    }

    const void* host_file_buf =
        mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, host_file_fd, 0);
    if (!host_file_buf) {
        panic("mmap failed");
    }

    close(host_file_fd);

    /* Init fat12 */
    const struct BPB* bpb = read_bpb();
    if (!bpb) {
        panic("Failed to read bpb");
    }

    /* Read FAT */
    size_t fat_size = bpb->sectors_per_fat * bpb->bytes_per_sector;
    uint8_t* fat_buffer = malloc(fat_size);
    bool ret = read_sectors(bpb, fat_buffer, 0x1, bpb->sectors_per_fat);
    if (!ret) {
        panic("Failed to read FAT");
    }

    test_open(bpb, fat_buffer, host_file_buf, st.st_size);
    test_complete_read(bpb, fat_buffer, host_file_buf, st.st_size);
    test_offset_read(bpb, fat_buffer, host_file_buf, st.st_size);
    test_truncated_read(bpb, fat_buffer, host_file_buf, st.st_size);
    test_large_read(bpb, fat_buffer, host_file_buf, st.st_size);

    free(fat_buffer);
    printf("heap_info: %lu, heap_free: %lu\n", heap_info(), heap_free);
    assert(heap_info() == heap_free);
    return 0;
}

