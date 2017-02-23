#include "../debug.h"
#include "../pmm.h"

void test_vmm()
{
    trace("Testing vmm");

    /* First 16Mb should be mapped and readable */
    for(uint32_t page = 0; page < 16 * 1024 * 1024; page += PAGE_SIZE) {
        unsigned* p = (unsigned*)page;
        unsigned val = *p;
    }

    /* After that, reading should yield a page fault */
    unsigned* p = (unsigned*)(16 * 1024 * 1024);
    p++;
    unsigned val = *p;

}


