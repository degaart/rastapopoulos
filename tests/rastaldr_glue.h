#pragma once

#include <stddef.h>
#include <stdint.h>

struct File;
struct BPB;

#define SYSCALL_READ 0
#define SYSCALL_WRITE 1
#define SYSCALL_OPEN  2
#define SYSCALL_LSEEK 8
#define SYSCALL_MMAP  9
#define SYSCALL_EXIT  60

#define PROT_READ  0x1
#define PROT_WRITE 0x2

#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

#define O_RDONLY 0

#define SEEK_SET 0

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

