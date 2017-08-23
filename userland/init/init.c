#include "runtime.h"
#include "debug.h"

#define invalid_code_path() \
    panic("Invalid code path")

void main()
{
    /* Start logger */
    int logger_pid = fork();
    if(!logger_pid) {
        exec("logger.elf");
        invalid_code_path();
    }

    for(int i= 0; i < 10; i++) {
        trace("Testing message-passing");
    }

#if 0
    /* Start vfs */
    int vfs_pid = fork();
    if(!vfs_pid) {
        exec("vfs.elf");
        invalid_code_path();
    }

    /* Try to read from initrd */
    int fd = open("init.c", O_RDONLY, 0);
    trace("Open: %d", fd);
#endif

}


