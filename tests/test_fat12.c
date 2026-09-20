#include "fat12.h"
#include "rastaldr.h"
#include "crc32.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

static int _fd;

//const char* __asan_default_options() { return "detect_leaks=0"; }

void _panic(const char* file, int line, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Panic at %s:%d: ", file, line);
    vfprintf(stderr, fmt, args);
    va_end(args);
    exit(1);
}

static bool read_sector(const struct BPB* bpb, void* buffer, uint16_t lba)
{
    ssize_t ret = pread(_fd, buffer, bpb->bytes_per_sector, lba * bpb->bytes_per_sector);
    if (ret != bpb->bytes_per_sector) {
        return false;
    }
    return true;
}

bool read_sectors(const struct BPB* bpb, void* buffer, uint16_t lba,
                  uint16_t count)
{
    uint8_t* ptr = buffer;
    while (count--) {
        if (!read_sector(bpb, ptr, lba)) {
            return false;
        }
        lba++;
        ptr += bpb->bytes_per_sector;
    }

    return true;
}

int main()
{
    _fd = open("../bootloader/build/rastapopoulos.img", O_RDONLY);
    if (_fd == -1) {
        panic("Failed to open disk image");
    }

    /* Read BPB */
    struct BPB bpb;
    ssize_t nread = pread(_fd, &bpb, sizeof(bpb), 0);
    if (nread < sizeof(bpb)) {
        panic("Failed to read BPB");
    }

    /* Read FAT */
    size_t fat_size = bpb.sectors_per_fat * bpb.bytes_per_sector;
    uint8_t* fat_buffer = malloc(fat_size);
    bool ret = read_sectors(&bpb, fat_buffer, 0x1, bpb.sectors_per_fat);
    if (!ret) {
        panic("Failed to read FAT");
    }

    struct File file;
    ret = fat12_open(&bpb, fat_buffer, &file, "kernel.elf");
    if (!ret) {
        panic("Failed to open kernel.elf");
    }

    char buffer[512];
    uint32_t crc = CRC32_INIT;
    while (true) {
        int nread = fat12_read(&file, buffer, sizeof(buffer));
        if (nread == -1) {
            panic("I/O error");
        } else if(nread == 0) {
            break;
        }

        fflush(stdout);

        crc = crc32_update(crc, buffer, nread);
    }
    fat12_close(&file);

    crc = crc32_finish(crc);
    assert(crc == 0x1CAD98B);
    printf("\ncrc32: 0x%08X\n", crc);
    free(fat_buffer);
    return 0;
}

