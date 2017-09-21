#include <stdlib.h>
#include <stdio.h>
#include <crc32.h>

int main()
{
    unsigned char buffer[32768];

    uint32_t crc = crc_init();
    while(1) {
        ssize_t ret = fread(buffer, 1, sizeof(buffer), stdin);
        if(ret < sizeof(buffer)) {
            if(ferror(stdin)) {
                fprintf(stderr, "Read error\n");
                return 1;
            } 
        } else if(ret == 0) {
            break;
        }

        crc = crc_update(crc, buffer, ret);
        if(feof(stdin))
            break;
    }
    crc = crc_finalize(crc);

    printf("0x%08X\n", crc);
    return 0;
}

