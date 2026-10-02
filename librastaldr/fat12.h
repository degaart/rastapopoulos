#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FAT12_FREE     0xe5
#define FAT12_FIRST    2
#define FAT12_RSVD     0xff0
#define FAT12_RSVD_END 0xff6
#define FAT12_BAD      0xff7
#define FAT12_END      0xff8
struct Dirent
{
    char filename[8];         /* Filename (padded with spaces) */
    char ext[3];              /* Extension (padded with spaces) */
    uint8_t attributes;       /* File attributes (hidden, system, dir, etc.) */
    uint8_t reserved_nt;      /* Reserved for NT (lowercase flags) */
    uint8_t creation_time_ms; /* Creation time tenths of a second */
    uint16_t creation_time;   /* Creation time (Hour:5, Min:6, Sec:5/2) */
    uint16_t creation_date;   /* Creation date (Year:7, Month:4, Day:5) */
    uint16_t last_access_date;  /* Last access date (Year:7, Month:4, Day:5) */
    uint16_t first_cluster_hi;  /* High 16-bits of cluster (0 in FAT12) */
    uint16_t write_time;        /* Last modification time */
    uint16_t write_date;        /* Last modification date */
    uint16_t first_cluster_low; /* Low 16-bits of cluster (Starting cluster) */
    uint32_t file_size;         /* File size in bytes */
} __attribute__((packed));

struct File
{
    const struct BPB* bpb;
    const uint8_t* fat;
    uint32_t size;
    uint32_t offset;
    uint32_t first_cluster;
    uint32_t cluster; /* current cluster loaded in buffer */
    void* buffer;
    uint32_t buffer_size;
    uint32_t buffer_offset;
};

bool fat12_open(const struct BPB* bpb, const void* fat, struct File* file,
                const char* filename);
int fat12_read(struct File* file, void* buffer, size_t size);
void fat12_close(struct File* file);
uint32_t fat12_seek(struct File* file, uint32_t offset);

