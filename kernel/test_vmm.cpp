#include "test_vmm.h"
#include "vmm.h"
#include "util.h"
#include "debug.h"

#define TEST_ADDRESS 0x400000

static void test_vmm_alloc() {
    TRACE("Testing VMM::alloc()");

    uint8_t* test_address = (uint8_t*)TEST_ADDRESS;
    bool ret = VMM::alloc(test_address, 1, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
    assert(ret == true);
    assert(VMM::is_mapped(test_address));

    VMM::dealloc(test_address, 1);
    assert(!VMM::is_mapped(test_address));

    assert(VMM::alloc(test_address, VMM::PAGE_SIZE * 16, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE));
    for(uint8_t* page = test_address; page < test_address + (VMM::PAGE_SIZE*16); page += VMM::PAGE_SIZE) {
        assert(VMM::is_mapped(page));
    }
    VMM::dealloc(test_address, VMM::PAGE_SIZE * 16);
    for(uint8_t* page = test_address; page < test_address + (VMM::PAGE_SIZE*16); page += VMM::PAGE_SIZE) {
        assert(!VMM::is_mapped(page));
    }

    assert(VMM::alloc(test_address, VMM::PAGE_SIZE * 16, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE));
    for(uint8_t* page = test_address; page < test_address + (VMM::PAGE_SIZE*16); page += VMM::PAGE_SIZE) {
        *page = 0;
    }
    VMM::dealloc(test_address, VMM::PAGE_SIZE * 16);
}

void test_vmm() {
    test_vmm_alloc();
}

