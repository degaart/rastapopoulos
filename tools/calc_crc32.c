#include "../bootloader/crc32.h"
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    char buffer[512];
    uint32_t crc = CRC32_INIT;
    while (1) {
        ssize_t r = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (r == -1) {
            perror("read");
            return 1;
        } else if (r == 0) {
            break;
        }

        crc = crc32_update(crc, buffer, (size_t)r);
        if (r < sizeof(buffer)) {
            break;
        }
    }
    crc = crc32_finish(crc);
    write(STDOUT_FILENO, &crc, sizeof(crc));
    return 0;
}

