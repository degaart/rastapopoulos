#pragma once

#include <stddef.h>
#include <stdint.h>

struct File;
struct BPB;

#define SYSCALL_READ  0
#define SYSCALL_WRITE 1
#define SYSCALL_OPEN  2
#define SYSCALL_CLOSE 3
#define SYSCALL_FSTAT 5
#define SYSCALL_LSEEK 8
#define SYSCALL_MMAP  9
#define SYSCALL_EXIT  60

#define PROT_READ  0x1
#define PROT_WRITE 0x2

#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

#define O_RDONLY 0

#define SEEK_SET 0

struct stat
{
    uint64_t st_dev;    /*  0:  8 */
    uint64_t st_ino;    /*  8:  8 */
    uint64_t st_nlink;  /* 16:  8 */
    uint32_t st_mode;   /* 24:  4 */
    uint32_t st_uid;    /* 28:  4 */
    uint32_t st_gid;    /* 32:  4 */
    uint32_t __pad0;    /* 36:  4 */
    uint64_t st_rdev;   /* 40:  8 */
    int64_t st_size;    /* 48:  8 */
    int64_t st_blksize; /* 56:  8 */
    int64_t st_blocks;  /* 64:  8 */
    struct
    {
        int64_t tv_sec;
        int64_t tv_nsec;
    } st_atime; /* 72: 16 */
    struct
    {
        int64_t tv_sec;
        int64_t tv_nsec;
    } st_mtime; /* 88: 16 */
    struct
    {
        int64_t tv_sec;
        int64_t tv_nsec;
    } st_ctime;          /* 104: 16 */
    int64_t __unused[3]; /* 120: 24 */
};

int64_t syscall4(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3,
                 uint64_t a4);
int64_t syscall6(int64_t nr, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4, uint64_t arg5, uint64_t arg6);

int open(const char* pathname, int flags);
void* mmap(void* addr, size_t length, int prot, int flags, int fd,
           size_t offset);
int read(int fd, void* buf, size_t count);
int lseek(int fd, int offset, int whence);
const struct BPB* read_bpb();
void read_fully(struct File* file, size_t offset, void* buffer, size_t size);
int fstat(int fd, struct stat* statbuf);
int close(int fd);

