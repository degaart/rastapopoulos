#ifndef _GDT_H_
#define _GDT_H_

#include <stdint.h>

class GDT {
public:
    static void init();
    static void dump();
    static void flush();
private:
    static void set_descriptor(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
};

#define KERNEL_CODE_SEG     0x08
#define KERNEL_DATA_SEG     0x10
#define USER_CODE_SEG       0x18
#define USER_DATA_SEG       0x20

#endif //_GDT_H_

