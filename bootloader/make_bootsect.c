#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * Two input files: boot sector and image file
 * Assert boot sector is 512 bytes exactly
 * What we do: copy 3 bytes from boot sector into start of image file
 * Seek to offset 62
 * Write the rest of the boot sector into image file
 */

static ssize_t write_all(int fd, void* buffer, size_t len)
{
    ssize_t result = 0;
    const char* ptr = buffer;
    while(len) {
        ssize_t w = write(fd, ptr, len);
        if(w == -1) {
            return -1;
        }
        len -= w;
        ptr += w;
        result += w;
    }
    return result;
}

int main(int argc, char** argv)
{
    if(argc < 3) {
        fprintf(stderr, "Usage: %s <bootsector> <image-file>\n", argv[0]);
        return 1;
    }

    const char* bootsector = argv[1];
    const char* image_file = argv[2];

    /* Check bootsector = 512 bytes */
    struct stat st;
    int ret = stat(bootsector, &st);
    if(ret == -1) {
        perror("stat");
        return 1;
    } else if(st.st_size != 512) {
        fprintf(stderr, "Boot sector has wrong size. Needs to be 512 bytes long\n");
        return 1;
    }

    /* Read bootsector into memory */
    int fd = open(bootsector, O_RDONLY);
    if(fd == -1) {
        perror("open");
        return 1;
    }
    char buffer[512];
    char* ptr = buffer;
    int remaining = sizeof(buffer);
    while(remaining > 0) {
        ssize_t r = read(fd, buffer, remaining);
        if(r == -1) {
            perror("read");
            return 1;
        } else if(r == 0) {
            fprintf(stderr, "Unexpected eof\n");
            return 1;
        }
        remaining -= r;
        ptr += r;
    }

    /* Open image file */
    close(fd);
    fd = open(image_file, O_WRONLY);
    if(fd == -1) {
        perror("open");
        return 1;
    }

    /* Write 3 bytes */
    ssize_t w = write_all(fd, buffer, 3);
    if(w == -1) {
        perror("write");
        return 1;
    }


    /* Seek to offset 62 */
    off_t sret = lseek(fd, 62, SEEK_SET);
    if(sret == -1) {
        perror("lseek");
        return 1;
    }

    /* Write the rest of the bootsector */
    ptr = buffer + 62;
    w = write_all(fd, ptr, sizeof(buffer) - 62);
    if(w == -1) {
        perror("write");
        return 1;
    }
    close(fd);
    return 0;
}

