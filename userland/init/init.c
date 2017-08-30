#include "runtime.h"
#include "debug.h"
#include <malloc.h>
#include <string.h>

void main()
{
    /* Start logger */
    int logger_pid = fork();
    if(!logger_pid) {
        exec("logger.elf");
        invalid_code_path();
    }

    /* Start vfs */
    int vfs_pid = fork();
    if(!vfs_pid) {
        exec("vfs.elf");
        invalid_code_path();
    }

    /* Start our block driver test */
    int blockdrv_pid = fork();
    if(!blockdrv_pid) {
        exec("blockdrv.elf");
        invalid_code_path();
    }

#if 0
    /* Try to read from initrd */
    int fd = open("init.c", O_RDONLY, 0);
    assert(fd != -1);

    char buffer[512];
    int ret;
    while(1) {
        ret = read(fd, buffer, sizeof(buffer));
        assert(ret != -1);
        if(ret == 0)
            break;

        debug_writen(buffer, ret);
    }
#endif
}

