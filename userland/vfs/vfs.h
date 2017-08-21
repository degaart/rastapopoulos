#pragma once

#define VFSPort         2

#define     VFSMessageOpen          0x001
#define     VFSMessageClose         0x002
#define     VFSMessageRead          0x003
#define     VFSMessageWrite         0x004
#define     VFSMessageResult        0x005

struct vfs_open_data {
    uint32_t mode;
    uint32_t perm;
    char filename[];
};

struct vfs_read_data {
    int fd;
    size_t size;
};

struct vfs_result_data {
    int result;
    char data[512];
};

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




