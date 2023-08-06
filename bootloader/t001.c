/*
    File access virtualization system

    - A file consists of one or more clusters
    - A cluster consists of one or more sectors
    - A sector consists of one or more bytes
    - Sectors can be loaded one at a time

    Provide a way to access a range of bytes from a file, given
    an offset into that file and a length. Pay attention to the
    case where the requested range straddles a sector boundary.
    We do not need to handle the case where the requested range
    exceeds the size of a sector.

    Unit test:
    - Generate a buffer of 512 bytes
    - Fill buffer with a predictable pattern: value is offset
    - Generate random length between 2 and 16
    - Generate random offset between 0 and 512 - length
    - Load the view from the file
    - Check the bytes are OK
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

unsigned char data[512];
unsigned bytes_per_sect = 32;
unsigned sect_per_clus = 2;

void* load_sector(unsigned sector)
{
    assert(sector < sizeof(data) / bytes_per_sect);
    return data + (sector * bytes_per_sect);
}

void* load_cluster(unsigned cluster, unsigned sector)
{
    unsigned cluster_count = sizeof(data) / (bytes_per_sect * sect_per_clus);

    assert(cluster < cluster_count);
    assert(sector < sect_per_clus);
    return data + (cluster * sect_per_clus * bytes_per_sect) +
           (sector * bytes_per_sect);
}

void load_view(void* dest_buffer, size_t len, unsigned offset)
{
    assert(len < bytes_per_sect);

    unsigned cluster_count = sizeof(data) / (bytes_per_sect * sect_per_clus);
    unsigned cluster = offset / (bytes_per_sect * sect_per_clus);
    // printf("cluster: %u\n", cluster);
    assert(cluster < cluster_count);
    assert(offset >= cluster * bytes_per_sect * sect_per_clus);
    assert(offset < (cluster + 1) * bytes_per_sect * sect_per_clus);

    unsigned sector =
        (offset - (cluster * bytes_per_sect * sect_per_clus)) / bytes_per_sect;
    // printf("sector: %u\n", sector);
    assert(sector < sect_per_clus);

    unsigned sector_offset = offset % bytes_per_sect;
    // printf("sector_offset: %u\n", sector_offset);
    assert(sector_offset < bytes_per_sect);

    /* Check for sector-straddling */
    if(sector_offset >= bytes_per_sect - len) {
        const unsigned char* buffer = load_cluster(cluster, sector);
        memcpy(dest_buffer, buffer + sector_offset,
               bytes_per_sect - sector_offset);
        if(sector + 1 < sect_per_clus) {
            buffer = load_cluster(cluster, sector + 1);
            memcpy(dest_buffer + bytes_per_sect - sector_offset, buffer,
                   len - bytes_per_sect + sector_offset);
        } else {
            buffer = load_cluster(cluster + 1, 0);
            memcpy(dest_buffer + bytes_per_sect - sector_offset, buffer,
                   len - bytes_per_sect + sector_offset);
        }
    } else {
        const unsigned char* buffer = load_cluster(cluster, sector);
        memcpy(dest_buffer, buffer + sector_offset, len);
    }
}

int main()
{
    srand(time(NULL));
    unsigned cluster_count = sizeof(data) / (bytes_per_sect * sect_per_clus);
    printf("bytes_per_sect: %u, sect_per_clus: %u, cluster_count: %u\n",
           bytes_per_sect, sect_per_clus, cluster_count);

    for(size_t i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }

    unsigned len = (rand() % 14) + 2;
    unsigned offset = rand() % (sizeof(data) - len);
    for(unsigned len = 1; len < 16; len++) {
        for(unsigned offset = 0; offset < sizeof(data) - len; offset++) {
            printf("len: %u, offset: %u\n", len, offset);
            unsigned char dest_buffer[16];
            load_view(dest_buffer, len, offset);
            for(size_t i = 0, j = offset; i < len; i++, j++) {
                // fprintf(stderr, "dest_buffer[%zu]: %u, data[%zu]: %u\n", i,
                // dest_buffer[i], j, data[j]);
                assert(dest_buffer[i] == data[j]);
            }
        }
    }
    return 0;
}
