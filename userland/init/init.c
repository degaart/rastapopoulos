#include <runtime.h>
#include <debug.h>
#include <malloc.h>
#include <string.h>
#include <crc32.h>

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
        crc = crc_finalize(crc);

        trace("init.c CRC32: 0x%04X", crc);
    }
}

static void run_tests()
{
    test_fat_read();
}

void main()
{
    /* Start logger */
    int logger_pid = fork();
    if(!logger_pid) {
        exec("logger.elf");
        invalid_code_path();
    }

    /* Start block driver */
    int blockdrv_pid = fork();
    if(!blockdrv_pid) {
        exec("blockdrv.elf");
        invalid_code_path();
    }

    /* Start vfs */
    int vfs_pid = fork();
    if(!vfs_pid) {
        exec("vfs.elf");
        invalid_code_path();
    }

    /* Run tests */
    run_tests();
}


