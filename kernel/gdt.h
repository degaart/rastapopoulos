#ifndef _GDT_H_
#define _GDT_H_

#include <stdint.h>

class GDT {
public:
    static void init();
    static void dump();
    static void flush();
    static void set_kernel_stack(const void* stack);
    static uint8_t* get_kernel_stack();

    static void tss_flush();
    static void set_iomap(int port);
    static void clear_iomap(int port);
    static bool test_iomap(int port);
private:
    static void set_descriptor(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
};

extern "C" 
void set_kernel_stack(const void* stack);

#define KERNEL_CODE_SEG     0x08
#define KERNEL_DATA_SEG     0x10
#define USER_CODE_SEG       0x18
#define USER_DATA_SEG       0x20
#define TSS_SEG             0x28

#define RPL0                0x0
#define RPL1                0x1
#define RPL2                0x2
#define RPL3                0x3

#endif //_GDT_H_

