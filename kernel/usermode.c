#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USER_CODE __attribute__((section(".user_text")))
#define USER_DATA __attribute__((section(".user_data")))
#define USER_RODATA __attribute__((section(".user_rodata")))

static const char message[] USER_RODATA = "Usermode started";

USER_CODE static void user_itoa(char* buffer, size_t size, unsigned value)
{
    if(value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char tmp[16];
    char* p = tmp;
    while(value) {
        *(p++) = (value % 10) + '0';
        value /= 10;
    }

    for(--p; p>=tmp && size > 1; size--) {
        *(buffer++) = *(p--);
    }

    *buffer = '\0';
}

USER_CODE static uint32_t syscall(uint32_t eax, uint32_t ebx,
                                  uint32_t ecx, uint32_t edx)
{
    uint32_t output;
    asm volatile("int 0x30\n"
                 "mov %0, eax\n"
                 : "=a"(output)
                 : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx));
    return output;
}

USER_CODE void user_entry(void)
{
#if 0
    volatile uint16_t* vga_base = (uint16_t*)0xB8000;
    volatile uint16_t* dst = vga_base;
    for(const char* p = message; *p; p++, dst++) {
        *dst = *p|(0x1E << 8);
    }

    asm volatile("int 0x30\n":::"memory");

    dst = vga_base + 80;
    for(const char* p = message2; *p; p++, dst++) {
        *dst = *p|(0x1B << 8);
    }
#else
    syscall(0, (uintptr_t)message, 0, 0);
    unsigned result = syscall(1, 2, 4, 8);
    if(result != 14)
        syscall(2, 0, 0, 0);
    syscall(3, 0, 0, 0);
#endif
    while(1);
}

