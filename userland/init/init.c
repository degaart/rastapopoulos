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

    /* 
     * Test block driver read functions 
     * read 64 + (512*2) + 64 blocks
     */
    uint64_t total_size = blockdrv_total_size();
    trace("Total size: %lld bytes", total_size);

    size_t buffer_size = 64 + (512 * 2) + 64;
    char* buffer = malloc(buffer_size);
    uint64_t offset = 0;
    unsigned crc = crc_init();

    while(offset < total_size) {
        size_t to_read = buffer_size;
        if(to_read > total_size - offset)
            to_read = total_size - offset;

        trace("Reading %d bytes at offset %lld", (int)to_read, offset);
        int ret = blockdrv_read(buffer, to_read, offset);
        if(ret == -1) {
            panic("Read failed");
        } else if(!ret) {
            panic("Premature end of disk");
        }

        crc = crc_update(crc, buffer, ret);

        offset += ret;
    }

    crc = crc_finalize(crc);
    trace("CRC32: 0x%08X", crc);


#if 0
    /* Start vfs */
    int vfs_pid = fork();
    if(!vfs_pid) {
        exec("vfs.elf");
        invalid_code_path();
    }
#endif
}

