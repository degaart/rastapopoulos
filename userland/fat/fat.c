#include <runtime.h>
#include <port.h>
#include <string.h>
#include <debug.h>
#include <crc32.h>
#include "ff.h"

void main()
{
    trace("Testing FAT support");

    FATFS fat;
    FRESULT fres;

    /* Mount drive 0 */
    memset(&fat, 0, sizeof(fat));
    fres = f_mount(&fat, "0:/", 1);
    if(fres != FR_OK) {
        panic("mount() failed: %d", fres);
    }

    /* List contents of root dir */
    DIR hdir;
    fres = f_opendir(&hdir, "/");
    if(fres != FR_OK) {
        panic("opendir() failed: %d", fres);
    }

    while(true) {
        FILINFO info;

        fres = f_readdir(&hdir, &info);
        if(fres != FR_OK) {
            panic("readdir() failed: %d", fres);
        } else if(info.fname[0] == '\0') {
            break;
        }

        if(info.fattrib & AM_DIR) {
            trace("[%s]",
                  info.fname);
        } else {
            trace("%s %lld",
                  info.fname,
                  (unsigned long long)info.fsize);
        }
    }

    f_closedir(&hdir);

    /* read init.c and calculate crc32 */
    FIL hfile;
    fres = f_open(&hfile, "init.c", FA_READ);
    if(fres != FR_OK) {
        panic("open() failed: %d", fres);
    }


    uint32_t crc = crc_init();
    while(true) {
        char buffer[64];
        unsigned read_bytes;

        fres = f_read(&hfile, buffer, sizeof(buffer), &read_bytes);
        if(fres != FR_OK) {
            panic("read() failed: %d", fres);
        }
        if(!read_bytes)
            break;

        crc = crc_update(crc, buffer, read_bytes);
    }
    crc = crc_finalize(crc);

    f_close(&hfile);

    trace("init.c crc32: 0x%04X", crc);
}





