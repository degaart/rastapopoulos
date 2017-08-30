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

#if 0
    /* Start vfs */
    int vfs_pid = fork();
    if(!vfs_pid) {
        exec("vfs.elf");
        invalid_code_path();
    }
#endif

    /* Start our block driver test */
    int blockdrv_pid = fork();
    if(!blockdrv_pid) {
        exec("blockdrv.elf");
        invalid_code_path();
    }

    /* Get sector count */
    uint32_t sector_count = blockdrv_sector_count();
    trace("Sector count: %d (%d Mb)",
          sector_count,
          sector_count / 2048);

    /* Read sectors and calculate crc of whole disk */
    uint32_t crc = crc_init();
    char buffer[512];
    for(uint32_t sector = 0; sector < sector_count; sector++) {
        if((sector % 1000) == 0)
            trace("Reading sector %d", sector);

        int ret = blockdrv_read(buffer, sizeof(buffer), sector);
        assert(ret == 0);

        crc = crc_update(crc, buffer, sizeof(buffer));
    }

    crc = crc_finalize(crc);
    trace("CRC32: 0x%X", crc);

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

