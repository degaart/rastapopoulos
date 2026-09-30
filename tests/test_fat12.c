#include "crc32.h"
#include "fat12.h"
#include "rastaldr.h"
#include "rastaldr_glue.h"
#include "allocator.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
    static const int HEAP_SIZE = 32768;
    void* heap = mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!heap) {
        panic("mmap failed");
    }

    heap_init(heap, HEAP_SIZE);

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

    /* Open kernel */
    struct File file;
    ret = fat12_open(bpb, fat_buffer, &file, "kernel.elf");
    if (!ret) {
        panic("Failed to open kernel.elf");
    }

    char buffer[512];
    uint32_t crc = CRC32_INIT;
    while (true) {
        int nread = fat12_read(&file, buffer, sizeof(buffer));
        if (nread == -1) {
            panic("I/O error");
        } else if (nread == 0) {
            break;
        }

        crc = crc32_update(crc, buffer, nread);
    }

    crc = crc32_finish(crc);
    assert(crc == 0x1CAD98B);
    printf("\ncrc32: 0x%08x\n", crc);

    fat12_seek(&file, 0);

    crc = CRC32_INIT;
    while (true) {
        int nread = fat12_read(&file, buffer, sizeof(buffer));
        if (nread == -1) {
            panic("I/O error");
        } else if (nread == 0) {
            break;
        }

        crc = crc32_update(crc, buffer, nread);
    }
    fat12_close(&file);

    crc = crc32_finish(crc);
    printf("\ncrc32: 0x%08x\n", crc);

    free(fat_buffer);
    return 0;
}

