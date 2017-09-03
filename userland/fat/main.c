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

    memset(&fat, 0, sizeof(fat));
    fres = f_mount(&fat, "0:/", 1);
    if(fres != FR_OK) {
        panic("mount() failed: %d", fres);
    }

    FIL hfile;
    fres = f_open(&hfile, "0:/init.c", FA_READ);
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





