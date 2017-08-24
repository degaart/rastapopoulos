#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "task_info.h"

extern unsigned char __START__[];
extern unsigned char __END__[];

#define _START_ ((unsigned char*)__START__)
#define _END_ ((unsigned char*)__END__)

/* Program control block */
struct pcb {
    int ack_port;
};

void debug_write(const char* str);
void debug_writen(const char* str, size_t count);
void yield();
int fork();
void sleep(unsigned ms);
void exit();
void reboot();
void send_ack(int port, unsigned code, uint32_t result);
void exec(const char* filename);

struct task_info;
bool get_task_info(int pid, struct task_info* buffer);

size_t initrd_get_size();
int initrd_read(void* dest, size_t size, size_t offset);

#define     PROT_NONE           0x0
#define     PROT_READ           0x1
#define     PROT_WRITE          0x2
#define     PROT_EXEC           0x4
void* mmap(void* addr, size_t size, uint32_t flags);

#define     O_RDONLY        0x1
#define     O_WRONLY        0x2
#define     O_RDWR          (O_RDONLY|O_WRONLY)
#define     O_CREAT         0x4
#define     O_TRUNC         0x8

#define     S_IRWXU         0700
#define     S_IRUSR         0400
#define     S_IWUSR         0200
#define     S_IXUSR         0100
#define     S_IRWXG         070
#define     S_IRGRP         040
#define     S_IWGRP         020
#define     S_IXGRP         010
#define     S_IRWXO         07
#define     S_IROTH         04
#define     S_IWOTH         02
#define     S_IXOTH         01

#define     MAX_PATH        260
int open(const char* filename, unsigned flags, int mode);
int read(int fd, void* buffer, size_t size);

extern int errno;

void* sbrk(ptrdiff_t incr);

