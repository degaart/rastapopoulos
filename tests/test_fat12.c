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

static void test_complete_read(const struct BPB* bpb, const void* fat_buffer)
{
    printf("*** Testing complete streaming read ***\n");

    /* Open kernel */
    struct File file;
    bool ret = fat12_open(bpb, fat_buffer, &file, "kernel.elf");
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
    assert(crc == 0x91cbd015);
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
    assert(crc == 0x91cbd015);
}

static void test_read_multiboot(const struct BPB* bpb,
                                const void* fat_buffer)
{
    printf("*** Testing multiboot read ***\n");

    struct File file;
    if (!fat12_open(bpb, fat_buffer, &file, "kernel.elf"))
    {
        panic("Failed to open kernel.elf\n");
    }

    size_t remaining = 8192;
    void* read_buffer = malloc(remaining);
    void* read_ptr = read_buffer;
    while (remaining) {
        int nread = fat12_read(&file, read_ptr, remaining);
        if (nread == -1) {
            panic("I/O error");
        } else if (nread == 0) {
            break;
        }

        printf("nread: %d\n", nread);

        remaining -= nread;
        read_ptr += nread;
    }

    /* Sliding-window search of the multiboot signature */
    const struct multiboot_header* hdr = NULL;
    for (read_ptr = read_buffer; read_ptr < read_buffer + 8192 - sizeof(struct multiboot_header); read_ptr += 4) {
        const struct multiboot_header* ptr = read_ptr;
        if (ptr->magic == MULTIBOOT_HEADER_MAGIC) {
            if (ptr->flags + ptr->magic + ptr->checksum != 0) {
                panic("Invalid multiboot checksum");
            }
            hdr = ptr;
            break;
        }
    }

    if (!hdr) {
        panic("Multiboot header not found");
    }
    if (hdr->flags != (MULTIBOOT_PAGE_ALIGN|MULTIBOOT_MEMORY_INFO)) {
        panic("Unsupported multiboot flags: 0x%lx", hdr->flags);
    }

    fat12_close(&file);
}

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

    test_complete_read(bpb, fat_buffer);
    test_read_multiboot(bpb, fat_buffer);

    free(fat_buffer);
    return 0;
}

