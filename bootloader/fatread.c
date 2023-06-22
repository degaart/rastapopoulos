#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FILENAME "MAKE_B~1   "

struct bpb {
    char jmp_boot[3];
    char oem_name[8];
    uint16_t bytes_per_sect;
    uint8_t sect_per_clus;
    uint16_t rsvd_sect_count;
    uint8_t fat_count;
    uint16_t root_ent_count;
    uint16_t total_sectors16;
    uint8_t media;
    uint16_t fat_size16;
    uint16_t sect_per_track;
    uint16_t num_heads;
    uint32_t hidden_sect_count;
    uint32_t total_sectors32;
    uint8_t drive_num;
    uint8_t reserved1;
    uint8_t bootsig;
    uint32_t volid;
    char label[11];
    char fstype[8];
} __attribute__((packed));

struct fat_entry {
    char name[11];
    uint8_t attrs;
    uint8_t reserved;
    uint8_t ctime_tenths;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    uint16_t first_cluster_hi; /* 0 for fat12 */
    uint16_t mtime;
    uint16_t mdate;
    uint16_t first_cluster_lo;
    uint32_t size;              /* in bytes */
} __attribute__((packed));

static ssize_t read_all(int fd, void* buffer, size_t len)
{
    ssize_t result = 0;
    char* ptr = buffer;
    while(len) {
        ssize_t r = read(fd, ptr, len);
        if(r == -1) {
            return -1;
        } else if(r == 0) {
            break;
        }
        len -= r;
        result += r;
        ptr += r;
    }
    return result;
}

int main(int argc, char** argv)
{
    assert(sizeof(struct bpb) == 62);
    assert(sizeof(struct fat_entry) == 32);

    if(argc < 3) {
        fprintf(stderr, "Usage: %s <image-file> <output-file>\n", argv[0]);
        return 1;
    }
    
    const char* image_file = argv[1];
    const char* output_file = argv[2];

    /* Read file into memory */
    int fd = open(image_file, O_RDONLY);
    if(fd == -1) {
        perror("open image file");
        return 1;
    }

    struct stat st;
    int ret = fstat(fd, &st);
    if(ret == -1) {
        perror("fstat image file");
        return 1;
    }
    size_t filesize = st.st_size;
    assert(filesize >= sizeof(struct bpb));
    void* buffer = malloc(filesize);
    ssize_t r = read_all(fd, buffer, filesize);
    if(r == -1) {
        perror("read image file");
        return 1;
    }
    close(fd);
    fd = -1;
    assert(r == filesize);

    struct bpb* bpb = buffer;
    printf("bytes per sector: %u\n"
           "sectors per cluster: %u\n"
           "fat size: %u\n"
           "reserved sector count: %u\n"
           "media: 0x%x\n",
           bpb->bytes_per_sect,
           bpb->sect_per_clus,
           bpb->fat_size16,
           bpb->rsvd_sect_count,
           bpb->media);

    /* Get root directory */
    size_t root_dir_sects = 
        ((bpb->root_ent_count * 32) + (bpb->bytes_per_sect - 1)) /
        bpb->bytes_per_sect;

    size_t first_data_sector = 
        bpb->rsvd_sect_count + 
        (bpb->fat_count * bpb->fat_size16) + 
        root_dir_sects;
    printf("first_data_sector: %zu (offset %zu)\n",
           first_data_sector,
           first_data_sector * bpb->bytes_per_sect);

    size_t first_root_dir_sect = first_data_sector - root_dir_sects;
    printf("first_root_dir_sect: 0x%zu (offset %zu)\n",
           first_root_dir_sect,
           first_root_dir_sect * bpb->bytes_per_sect);

    /* Find file in root directory */
    int found = 1;
    const struct fat_entry* entry = (const struct fat_entry*)
        (((const unsigned char*)buffer) +
         (first_root_dir_sect * bpb->bytes_per_sect));
    while(entry->name[0]) {
        if(!memcmp(entry->name, FILENAME, sizeof(FILENAME) -1)) {
            found++;
            break;
        }
        entry++;
    }
    if(!found) {
        fprintf(stderr, "File not found\n");
        return 1;
    }
    printf("file found at cluster %u (size: %u)\n",
           entry->first_cluster_lo, entry->size);

    /* Open output file */
    fd = open(output_file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(fd == -1) {
        perror("open output file");
        printf("output file: %s\n", output_file);
        return 1;
    }

    /* Follow cluster chain to read file */
    uint32_t remaining = entry->size;
    size_t next_cluster = entry->first_cluster_lo;
    unsigned cluster_index = 0;
    while(remaining) {
        size_t write_size = remaining > bpb->bytes_per_sect ? bpb->bytes_per_sect : remaining;
        size_t sector = ((next_cluster - 2) * bpb->sect_per_clus) + first_data_sector;
        const unsigned char* src = (const unsigned char*)buffer + (sector * bpb->bytes_per_sect);
        ssize_t w = write(fd, src, write_size);
        if(w == -1) {
            perror("write output file");
        } else if(w < write_size) {
            fprintf(stderr, "I/O error\n");
            return 1;
        }
        remaining -= w;

        size_t fat_offset = next_cluster + (next_cluster / 2);
        size_t fat_sector = bpb->rsvd_sect_count + (fat_offset / bpb->bytes_per_sect);
        size_t entry_offset = fat_offset % bpb->bytes_per_sect;
        const unsigned char* fat_buffer = (const unsigned char*)buffer +
            (bpb->rsvd_sect_count * bpb->bytes_per_sect);
        uint16_t fat_value = *((uint16_t*)(fat_buffer + entry_offset));
        if(next_cluster & 0x01)
            fat_value = fat_value >> 4;
        else
            fat_value = fat_value & 0x0FFF;

        printf("Cluster %02u: 0x%03x\n", cluster_index, fat_value);
        if(fat_value >= 0xFF8) {
            /* No moar clusters */
            if(remaining) {
                fprintf(stderr, "FAT inconsistency detected! Invalid cluster count\n");
                return 1;
            }
            break;
        } else if(fat_value == 0xFF7) {
            /* Bad cluster. Error out */
            fprintf(stderr, "I/O error (bad cluster)\n");
            return 1;
        } else {
            /* fat_value is next cluster */
            next_cluster = fat_value;
        }
        cluster_index++;
    }
    return 0;
}


