#pragma once

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




