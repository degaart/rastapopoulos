#include "a20.h"

/*
 * Check A20 is enabled by writing temporary values
 * to 0000:0500 and checking whether it does not alias
 * FFFF:0500
 */
bool a20_enabled(void)
{
    unsigned int result;

    __asm__ volatile("push ds\n\t"
                     "push es\n\t"

                     "xor ax, ax\n\t"
                     "mov ds, ax\n\t"

                     "mov ax, 0xffff\n\t"
                     "mov es, ax\n\t"

                     "mov al, BYTE PTR ds:[0x0500]\n\t"
                     "push ax\n\t"

                     "mov al, BYTE PTR es:[0x0510]\n\t"
                     "push ax\n\t"

                     "mov BYTE PTR ds:[0x0500], 0x00\n\t"
                     "mov BYTE PTR es:[0x0510], 0xff\n\t"

                     "xor ax, ax\n\t"
                     "cmp BYTE PTR ds:[0x0500], 0x00\n\t"
                     ".byte 0x0f, 0x94, 0xc0\n\t" /* sete al */
                     "mov dx, ax\n\t"

                     "pop ax\n\t"
                     "mov BYTE PTR es:[0x0510], al\n\t"

                     "pop ax\n\t"
                     "mov BYTE PTR ds:[0x0500], al\n\t"

                     "mov ax, dx\n\t"

                     "pop es\n\t"
                     "pop ds\n\t"

                     : "=a"(result)
                     :
                     : "dx", "cc", "memory");

    return result;
}

/*
 * Enable A20 by calling bios int 15h
 */
bool a20_enable_bios(void)
{
    unsigned int result;

    __asm__ volatile("mov ax, 0x2401\n\t"
                     "int 0x15\n\t"

                     /*
                      *   CF clear = success
                      *   CF set   = failure
                      */
                     "mov ax, 0\n\t"
                     "jc 1f\n\t"
                     "inc ax\n\t"
                     "1:\n\t"

                     : "=a"(result)
                     :
                     : "cc", "memory");

    return result;
}

/*
 * Enable A20 by writing to port 0x92 (fast A20)
 */
void a20_enable_fast(void)
{
    __asm__ volatile("in al, 0x92\n\t"
                     "or al, 0x02\n\t"  /* set A20 enable */
                     "and al, 0xFE\n\t" /* keep fast-reset bit clear */
                     "out 0x92, al\n\t"
                     :
                     :
                     : "ax", "cc");
}

bool a20_enable_8042(void)
{
    unsigned int result;

    __asm__ volatile(
        /*
         * Wait until 8042 input buffer is empty.
         * Then disable keyboard with command ADh.
         */
        "mov cx, 0xffff\n\t"
        "1:\n\t"
        "in al, 0x64\n\t"
        "test al, 0x02\n\t"
        "jz 2f\n\t"
        "loop 1b\n\t"
        "jmp 9f\n\t"

        "2:\n\t"
        "mov al, 0xAD\n\t"
        "out 0x64, al\n\t"

        /*
         * Wait until input buffer is empty,
         * then request the output port with D0h.
         */
        "mov cx, 0xffff\n\t"
        "3:\n\t"
        "in al, 0x64\n\t"
        "test al, 0x02\n\t"
        "jz 4f\n\t"
        "loop 3b\n\t"
        "jmp 9f\n\t"

        "4:\n\t"
        "mov al, 0xD0\n\t"
        "out 0x64, al\n\t"

        /*
         * Wait until output buffer contains the output-port value.
         */
        "mov cx, 0xffff\n\t"
        "5:\n\t"
        "in al, 0x64\n\t"
        "test al, 0x01\n\t"
        "jnz 6f\n\t"
        "loop 5b\n\t"
        "jmp 9f\n\t"

        "6:\n\t"
        "in al, 0x60\n\t"
        "mov ah, al\n\t" /* preserve output-port value */

        /*
         * Wait until input buffer is empty,
         * then issue D1h (write output port).
         */
        "mov cx, 0xffff\n\t"
        "7:\n\t"
        "in al, 0x64\n\t"
        "test al, 0x02\n\t"
        "jz 8f\n\t"
        "loop 7b\n\t"
        "jmp 9f\n\t"

        "8:\n\t"
        "mov al, 0xD1\n\t"
        "out 0x64, al\n\t"

        /*
         * Wait until we can write the new output-port value.
         */
        "mov cx, 0xffff\n\t"
        "10:\n\t"
        "in al, 0x64\n\t"
        "test al, 0x02\n\t"
        "jz 11f\n\t"
        "loop 10b\n\t"
        "jmp 9f\n\t"

        "11:\n\t"
        "mov al, ah\n\t"
        "or al, 0x02\n\t" /* enable A20 */
        "out 0x60, al\n\t"

        /*
         * Wait until the write has completed.
         */
        "mov cx, 0xffff\n\t"
        "12:\n\t"
        "in al, 0x64\n\t"
        "test al, 0x02\n\t"
        "jz 13f\n\t"
        "loop 12b\n\t"
        "jmp 9f\n\t"

        /*
         * Re-enable keyboard with command AEh.
         */
        "13:\n\t"
        "mov al, 0xAE\n\t"
        "out 0x64, al\n\t"

        /*
         * Wait for AEh to be accepted as well.
         */
        "mov cx, 0xffff\n\t"
        "14:\n\t"
        "in al, 0x64\n\t"
        "test al, 0x02\n\t"
        "jz 15f\n\t"
        "loop 14b\n\t"
        "jmp 9f\n\t"

        /* Success */
        "15:\n\t"
        "mov ax, 1\n\t"
        "jmp 16f\n\t"

        /* Timeout */
        "9:\n\t"
        "xor ax, ax\n\t"

        "16:\n\t"

        : "=a"(result)
        :
        : "cx", "cc", "memory");

    return result;
}

