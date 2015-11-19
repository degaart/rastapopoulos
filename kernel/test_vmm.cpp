#include "test_vmm.h"
#include "vmm.h"
#include "pmm.h"
#include "util.h"
#include "debug.h"
#include "kmalloc.h"

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

static void test_clone() {
    TRACE("Testing clone_pagedir()");

    uint32_t* test_addr = (uint32_t*)TEST_ADDRESS;

    Pagedir* current_pagedir = VMM::create_pagedir();
    VMM::switch_pagedir(current_pagedir);

    assert(VMM::alloc(test_addr, VMM::PAGE_SIZE * 16, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER));
    for(unsigned dword = 0; dword < (VMM::PAGE_SIZE * 16) / sizeof(unsigned); dword++) {
        test_addr[dword] = dword ^ 0x7;
    }

    Pagedir* clone = VMM::clone_pagedir();
    assert(clone);
    
    // kernel-space should not be mapped at this point
    for(unsigned page = 0; page < TEST_ADDRESS; page += VMM::PAGE_SIZE)
        assert(!clone->is_mapped(page));

    // but user-space should be mapped
    for(unsigned page = TEST_ADDRESS; page < TEST_ADDRESS + (VMM::PAGE_SIZE*16); page += VMM::PAGE_SIZE)
        assert(clone->is_mapped(page));

    // switch to cloned pagedir and check all values are correct
    VMM::switch_pagedir(clone);
    for(unsigned dword = 0; dword < (VMM::PAGE_SIZE * 16) / sizeof(unsigned); dword++) {
        assert(test_addr[dword] == (dword ^ 0x7));
    }

    // Change values
    for(unsigned dword = 0; dword < (VMM::PAGE_SIZE * 16) / sizeof(unsigned); dword++)
        test_addr[dword] = dword;

    // switch back to initial pagedir
    VMM::switch_pagedir(current_pagedir);

    // Check values should not have changed at all
    for(unsigned dword = 0; dword < (VMM::PAGE_SIZE * 16) / sizeof(unsigned); dword++)
        assert(test_addr[dword] == (dword ^ 0x7));    
}

/*
    scratch: pointer to kernel-space unmapped adddress of 4096 bytes, page-aligned
*/
extern "C" void _flush_tlb(void* va);
static void copy_frame(Pagedir* dst, void* address, void* scratch) {
    assert(!((uint32_t)address % VMM::PAGE_SIZE));
    assert(!((uint32_t)scratch % VMM::PAGE_SIZE));

    uint32_t frame = PMM::alloc();
    TRACE("Copying %p (frame 0x%X)", address, frame);

    VMM::map(scratch, frame, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);

    memcpy(scratch, address, VMM::PAGE_SIZE);
    VMM::unmap(scratch);

    dst->map(address, frame, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER);
}

static void test_clone1() {
    TRACE("Testing clone");

    uint32_t* test_addr = (uint32_t*)TEST_ADDRESS;

    uint32_t* scratch = (uint32_t*)kmalloc_a(VMM::PAGE_SIZE, VMM::PAGE_SIZE);
    VMM::unmap(scratch);

    Pagedir* pagedir0 = VMM::create_pagedir();
    VMM::switch_pagedir(pagedir0);
    assert(VMM::alloc(test_addr, VMM::PAGE_SIZE * 2, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER));
    for(unsigned dword = 0; dword < (VMM::PAGE_SIZE*2) / sizeof(unsigned); dword++)
        test_addr[dword] = dword ^ 0x12345678;

    Pagedir* pagedir1 = VMM::create_pagedir();
    copy_frame(pagedir1, test_addr, scratch);
    copy_frame(pagedir1, ((uint8_t*)test_addr)+VMM::PAGE_SIZE, scratch);
    VMM::switch_pagedir(pagedir1);

    for(unsigned dword = 0; dword < (VMM::PAGE_SIZE*2) / sizeof(unsigned); dword++) {
        if(test_addr[dword] != (dword ^ 0x12345678))
            PANIC("difference found at %u: 0x%X, 0x%X", dword, dword ^ 0x12345678, test_addr[dword]);
        // assert(test_addr[dword] == (dword ^ 0x12345678));
    }

    for(unsigned dword = 0; dword < (VMM::PAGE_SIZE*2) / sizeof(unsigned); dword++)
        test_addr[dword] = dword ^ 0x87654321;

    VMM::switch_pagedir(pagedir0);
    for(unsigned dword = 0; dword < (VMM::PAGE_SIZE*2) / sizeof(unsigned); dword++)
        assert(test_addr[dword] == (dword ^ 0x12345678));    

    VMM::alloc(scratch, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
}

void test_vmm() {
    //test_vmm_alloc();
     test_clone();
    // test_clone1();
}

