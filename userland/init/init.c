#include <runtime.h>
#include <debug.h>
#include <malloc.h>
#include <string.h>
#include <crc32.h>

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

    int fat_pid = fork();
    if(!fat_pid) {
        exec("fat.elf");
        invalid_code_path();
    }

#if 0
    /* Start vfs */
    int vfs_pid = fork();
    if(!vfs_pid) {
        exec("vfs.elf");
        invalid_code_path();
    }
#endif
}

