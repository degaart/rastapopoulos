#include <rasta.h>
#include <stdio.h>
#include <string.h>

#define TEST_SECTOR 2878u

static uint32_t random_next(uint32_t* state)
{
    *state = (0x3623u + *state * 0x625fu) % 0xfff1u;
    return *state;
}

int main(void)
{
    unsigned generation;
    if (sys_fdc_check_media(0, &generation) != 0) {
        printf("Cannot check floppy 0 media generation\n");
        return 1;
    }
    printf("Floppy 0 generation: %u\n", generation);

    uint32_t rng = (uint32_t)(sys_getticks() % 0xfff1u);
    char written[512];
    char received[512];
    for (unsigned i = 0; i < sizeof(written); i++)
        written[i] = random_next(&rng);
    printf("Writing floppy 0 sector %u\n", TEST_SECTOR);
    if (sys_fdc_write(0, TEST_SECTOR, written) != 0) {
        printf("Sector write failed\n");
        return 1;
    }
    if (sys_fdc_read(0, TEST_SECTOR, received) != 0) {
        printf("Sector read failed\n");
        return 1;
    }
    if (memcmp(written, received, sizeof(written)) != 0) {
        printf("Sector contents do not match\n");
        return 1;
    }
    printf("Sector contents verified\n");
    if (sys_fdc_check_media(0, &generation) != 0) {
        printf("Cannot establish generation before media change\n");
        return 1;
    }
    printf("Generation before swap: %u\n", generation);
    struct KeyEvent event;
    int overflow;
    while (sys_get_kbd_event(&event, &overflow, 0) == 0) {
    }
    printf("Change the floppy in drive 0, then press a key\n");
    do {
        if (sys_get_kbd_event(&event, &overflow, 1) != 0) {
            printf("Keyboard input failed\n");
            return 1;
        }
    } while (event.type != KEY_PRESSED);
    unsigned after;
    if (sys_fdc_check_media(0, &after) != 0) {
        printf("Cannot check generation after media change\n");
        return 1;
    }
    printf("Floppy 0 generation: %u; %s\n", after,
           after != generation ? "changed" : "unchanged");
    return 0;
}
