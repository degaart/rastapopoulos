#pragma once

#include <stdbool.h>
#include <stdint.h>

void _panic(const char* file, int line, const char* fmt, ...) __attribute__((format(printf, 3, 4), noreturn));
#define panic(...) _panic(__FILE__, __LINE__, __VA_ARGS__)

struct BPB
{
    uint8_t jump[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t boot_drive;
    uint8_t reserved;
    uint8_t extended_signature;
    uint32_t volume_serial;
    uint8_t volume_label[11];
    uint8_t filesystem_type[8];
} __attribute__((packed));

bool read_sectors(const struct BPB* bpb, void* buffer, uint16_t lba,
                  uint16_t count);

