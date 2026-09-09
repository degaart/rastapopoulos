#include <stddef.h>
#include <stdint.h>

char testBss[512];

extern void puts(const char* msg);
extern void halt() __attribute__((noreturn));

void ldrmain(void)
{
    for (int i = 0; i < sizeof(testBss); i++)
    {
        if(testBss[i])
        {
            puts("Bss not cleared\r\n");
            halt();
        }
    }
    puts("ldrmain running\r\n");
    while(1);
    halt();
}


