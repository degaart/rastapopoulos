#include "fat12.h"
#include "rastaldr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool read_cluster(const struct BPB* bpb, void* buffer, unsigned cluster)
{
    if (cluster < FAT12_FIRST || cluster == FAT12_BAD ||
        (cluster >= FAT12_RSVD && cluster <= FAT12_RSVD_END) ||
        (cluster >= FAT12_END)) {
        printf("Invalid cluster detected\n");
        return false;
    }

    uint16_t first_data_lba =
        bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat) +
        ((bpb->root_entries * 32 + bpb->bytes_per_sector - 1) /
         bpb->bytes_per_sector);
    uint16_t lba = first_data_lba + (cluster - 2) * bpb->sectors_per_cluster;

    return read_sectors(bpb, buffer, lba, bpb->sectors_per_cluster);
}

bool fat12_open(const struct BPB* bpb, const void* fat, struct File* file,
                const char* filename)
{
    /* transform filename */
    char name_buffer[8 + 3];
    memset(name_buffer, ' ', sizeof(name_buffer));

    char* ext = strrchr(filename, '.');
    if (ext) {
        if (strlen(ext) > 4) {
            return false;
        } else if (ext - filename > 8) {
            return false;
        }
        memcpy(name_buffer, filename, ext - filename);
        memcpy(name_buffer + 8, ext + 1, strlen(ext) - 1);
    } else {
        if (strlen(filename) > 8) {
            return false;
        }
        memcpy(name_buffer, filename, strlen(filename));
    }

    for (int i = 0; i < 8 + 3; i++) {
        if (name_buffer[i] >= 'a' && name_buffer[i] <= 'z') {
            name_buffer[i] = name_buffer[i] - 'a' + 'A';
        }
    }

    /* read root dir */
    unsigned root_dir_lba =
        bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
    unsigned root_dir_sectors =
        (bpb->root_entries * 32) / bpb->bytes_per_sector;
    struct Dirent* root_dir_buffer =
        malloc(root_dir_sectors * bpb->bytes_per_sector);
    bool ret =
        read_sectors(bpb, root_dir_buffer, root_dir_lba, root_dir_sectors);
    if (!ret) {
        panic("Failed to read root dir");
    }

    /* Find in root dir */
    const struct Dirent* entry = root_dir_buffer;
    const struct Dirent* file_entry = NULL;
    for (; entry->filename[0]; entry++) {
        if (entry->filename[0] == FAT12_FREE) {
            continue;
        } else if (!memcmp(entry->filename, name_buffer,
                           sizeof(name_buffer))) {
            file_entry = entry;
            break;
        }
    }

    if (!file_entry) {
        free(root_dir_buffer);
        printf("File not found: %s\n", filename);
        return false;
    } else if (entry->first_cluster_hi) {
        free(root_dir_buffer);
        printf("Unsupported first cluster for file: %s\n", filename);
        return false;
    }

    file->size = entry->file_size;
    file->offset = 0;
    file->first_cluster = (uint32_t)entry->first_cluster_low |
                          ((uint32_t)entry->first_cluster_hi << 16);
    file->cluster = entry->first_cluster_low;
    file->bpb = bpb;
    file->fat = fat;
    file->buffer_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;
    file->buffer = malloc(file->buffer_size);
    file->buffer_offset = 0;

    if (!read_cluster(file->bpb, file->buffer, file->cluster)) {
        free(root_dir_buffer);
        printf("Failed to read cluster 0x%x\n", file->cluster);
        return -1;
    }
    free(root_dir_buffer);
    return true;
}

static uint32_t next_cluster(const uint8_t* fat, uint32_t cluster)
{
    size_t offset = cluster + (cluster / 2);
    uint16_t value = fat[offset] | ((uint16_t)fat[offset + 1] << 8);
    if (cluster & 1) { /* odd */
        cluster = value >> 4;
    } else { /* even */
        cluster = value & 0xfff;
    }
    return cluster;
}

int fat12_read(struct File* file, void* buffer, size_t size)
{
    /* adjust size for EOF */
    if (size > file->size - file->offset) {
        size = file->size - file->offset;
    }

    int result = 0;
    while (size) {
        uint32_t buffer_avail = file->buffer_size - file->buffer_offset;
        if (buffer_avail > 0) {
            int nread = size <= buffer_avail ? size : buffer_avail;
            if (nread > file->size - file->offset)
                nread = file->size - file->offset;
            memcpy(buffer, file->buffer + file->buffer_offset, nread);
            buffer += nread;
            size -= nread;
            file->offset += nread;
            file->buffer_offset += nread;
            result += nread;
        }

        if (file->buffer_offset >= file->buffer_size) {
            /* fill buffer again */
            file->cluster = next_cluster(file->fat, file->cluster);
            if (file->cluster >= FAT12_END) {
                break;
            }

            if (!read_cluster(file->bpb, file->buffer, file->cluster)) {
                printf("Failed to read cluster 0x%x\n", file->cluster);
                return -1;
            }
            file->buffer_offset = 0;
        }
    }
    return result;
}

void fat12_close(struct File* file)
{
    if (file->buffer) {
        free(file->buffer);
    }
}

uint32_t fat12_seek(struct File* file, uint32_t offset)
{
    /* TODO: optimization: Do not start at the first cluster if offset >=
     * file->offset */
    uint32_t cluster_size =
        file->bpb->sectors_per_cluster * file->bpb->bytes_per_sector;

    file->offset = 0;
    file->cluster = file->first_cluster;
    while (offset >= cluster_size) {
        file->cluster = next_cluster(file->fat, file->cluster);
        if (file->cluster >= FAT12_END) {
            break;
        }
        file->offset += cluster_size;
        offset -= cluster_size;
    }

    if (file->cluster < FAT12_END) {
        if (!read_cluster(file->bpb, file->buffer, file->cluster)) {
            printf("Failed to read cluster 0x%x\n", file->cluster);
            return -1;
        }
    }

    if (file->offset + offset > file->size) {
        offset = file->size - file->offset;
    }

    file->offset += offset;
    file->buffer_offset = offset;
    return file->offset;
}

