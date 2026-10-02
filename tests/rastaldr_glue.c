#include "rastaldr_glue.h"
#include "fat12.h"
#include "format.h"
#include <rastaldr.h>
#include <stddef.h>
#include <stdlib.h>

int _fd;

const struct BPB* read_bpb()
{
    _fd = open("../bootloader/build/rastapopoulos.img", O_RDONLY);
    if (_fd == -1) {
        panic("Failed to open disk image");
    }

    struct BPB* bpb = malloc(sizeof(struct BPB));
    int nread = read(_fd, bpb, sizeof(struct BPB));
    if (nread != sizeof(struct BPB)) {
        return NULL;
    }
    return bpb;
}

void read_fully(struct File* file, size_t offset, void* buffer, size_t size)
{
    if (fat12_seek(file, offset) != offset) {
        panic("Seek failed");
    }

    void* ptr = buffer;
    while (size) {
        int nread = fat12_read(file, ptr, size);
        if (nread == -1) {
            panic("I/O error");
        } else if (nread == 0) {
            panic("Unexpected EOF");
        }

        size -= nread;
        ptr += nread;
    }
}

int putchar(int ch)
{
    syscall4(SYSCALL_WRITE, 1, /* stdout */
             (uint64_t)&ch,    /* buffer */
             1,                /* count */
             0);
    return ch;
}

int64_t syscall6(int64_t nr, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    register uint64_t r10 __asm__("r10") = arg4;
    register uint64_t r8 __asm__("r8") = arg5;
    register uint64_t r9 __asm__("r9") = arg6;

    int64_t ret;

    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(nr), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10),
                       "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory");

    return ret;
}

int64_t syscall4(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3,
                 uint64_t a4)
{
    register int64_t r10 __asm__("r10") = (int64_t)a4;
    long ret;

    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
                     : "rcx", "r11", "memory");

    return ret;
}

static void write_stderr(void* data, char ch)
{
    syscall4(SYSCALL_WRITE, 2, /* stderr */
             (uint64_t)&ch,    /* buffer */
             1,                /* count */
             0);
}

void _panic(const char* file, int line, const char* fmt, ...)
{
    format(write_stderr, NULL, "PANIC at %s:%d:\n", file, line);
    va_list args;
    va_start(args, fmt);
    vformat(write_stderr, NULL, fmt, args);
    va_end(args);

    putchar('\n');

    syscall4(SYSCALL_EXIT, 1, 0, 0, 0);
    while (1)
        ;
}

int lseek(int fd, int offset, int whence)
{
    int64_t ret = syscall4(SYSCALL_LSEEK, fd, offset, whence, 0);
    if (ret < 0) {
        return -1;
    }
    return (int)ret;
}

bool read_sector(const struct BPB* bpb, void* buffer, unsigned lba)
{
    if (lseek(_fd, lba * bpb->bytes_per_sector, SEEK_SET) == -1) {
        return false;
    }

    int remaining = bpb->bytes_per_sector;
    while (remaining > 0) {
        int nread = read(_fd, buffer, bpb->bytes_per_sector);
        if (nread == -1 || nread == 0) {
            return false;
        }

        remaining -= nread;
        buffer += nread;
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

void* mmap(void* addr, size_t length, int prot, int flags, int fd,
           size_t offset)
{
    int64_t ret = syscall6(SYSCALL_MMAP, (uint64_t)addr, /* addr */
                           length,                       /* length */
                           prot,                         /* prot */
                           flags,                        /* flags */
                           fd,                           /* fd */
                           offset /* offset */);
    if (ret < 0) {
        return NULL;
    }
    return (void*)ret;
}

int open(const char* pathname, int flags)
{
    int64_t ret = syscall4(SYSCALL_OPEN, (uint64_t)pathname, flags, 0, 0);
    if (ret < 0) {
        return -1;
    }
    return ret;
}

int read(int fd, void* buf, size_t count)
{
    int64_t ret = syscall4(SYSCALL_READ, fd, (uint64_t)buf, count, 0);
    if (ret < 0) {
        return -1;
    }
    return ret;
}

int fstat(int fd, struct stat* statbuf)
{
    int64_t ret = syscall4(SYSCALL_FSTAT, fd, (uint64_t)statbuf, 0, 0);
    if (ret < 0) {
        return -1;
    }
    return (int)ret;
}

int close(int fd)
{
    int64_t ret = syscall4(SYSCALL_CLOSE, fd, 0, 0, 0);
    if (ret < 0) {
        return -1;
    }
    return (int)ret;
}

