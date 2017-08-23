#include "runtime.h"
#include "debug.h"

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
}

