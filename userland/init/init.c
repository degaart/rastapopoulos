#include <runtime.h>
#include <debug.h>
#include <malloc.h>
#include <string.h>
#include <crc32.h>
#include <port.h>


static void test_fat_read()
{
    trace("Starting FAT read tests");

    int fd = open("/init.c", O_RDONLY, 0);
    trace("fd: %d", fd);

    if(fd != -1) {
        char buffer[512];
        uint32_t crc = crc_init();
        while(true) {
            int read_bytes = read(fd, buffer, sizeof(buffer));
            if(read_bytes == -1) {
                panic("I/O error");
            } else if(read_bytes == 0) {
                break;
            } else {
                crc = crc_update(crc, buffer, read_bytes);
            }
        }
        int ret = close(fd);
        assert(ret == 0);

        crc = crc_finalize(crc);
        trace("init.c CRC32: 0x%04X", crc);
    }
}

/*
 * Read from fat then display on screen
 */
static void test_read_print()
{
    int fd = open("/still-alive.txt", O_RDONLY, 0);
    if(fd == -1) {
        panic("open() failed");
        return;
    }

    while(1) {
        char buffer[513];
        int ret = read(fd, buffer, sizeof(buffer) - 1);
        if(ret == -1) {
            panic("read() failed");
            return;
        } else if(ret == 0) {
            break;
        }
        buffer[ret] = '\0';
        puts(buffer);
    }

    close(fd);
    trace("test_read_log done");
}

static void run_tests()
{
#if 0
    test_fat_read();
#else
    test_read_print();
#endif
}

void main()
{
#if 1
    /* Start block driver */
    int blockdrv_pid = fork();
    if(!blockdrv_pid) {
        exec("blk.elf");
        invalid_code_path();
    }

    /* Start vfs */
    int vfs_pid = fork();
    if(!vfs_pid) {
        exec("vfs.elf");
        invalid_code_path();
    }
#endif

#if 0
    /* Start svga driver */
    int svga_pid = fork();
    if(!svga_pid) {
        exec("svga.elf");
        invalid_code_path();
    }
#else
    /* Start vga driver */
    int vga_pid = fork();
    if(!vga_pid) {
        exec("vga.elf");
        invalid_code_path();
    }
#endif

#if 1
    /* Run tests */
    run_tests();
    while(1);
#endif
}


