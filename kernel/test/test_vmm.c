#include "../debug.h"
#include "../pmm.h"
#include "../vmm.h"
#include "../registers.h"
#include "../kernel.h"
#include "../util.h"
#include "../idt.h"
#include "../kmalloc.h"

#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3ff)
#define PAGE_GET_PHYSICAL_ADDRESS(x) (*x & ~0xfff)

static bool is_identity_mapped(void* address)
{
    void* frame = (void*) vmm_get_physical(address);
    return frame == address;
}

static void test_transient_map()
{
    trace("Testing transient maps");

    /* Allocate frame, and create transient map */
    uint32_t frame0 = pmm_alloc();
    uint32_t* map0 = vmm_transient_map(frame0, VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE);

    /* Fill with predictable data */
    for(int i = 0; i < 1024; i++)
        map0[i] = i + 10000;

    /* Create another transient map to same frame */
    uint32_t* map1 = vmm_transient_map(frame0, VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE);

    /* Check data */
    for(int i = 0; i < 1024; i++)
        assert(map1[i] == i + 10000);

    /* scramble data in second map */
    for(int i = 0; i < 1024; i++)
        map1[i] = i ^ 7;

    /* Which should change first map */
    for(int i = 0; i < 1024; i++)
        assert(map0[i] == (i ^ 7));

    /* Unmap first map */
    vmm_transient_unmap(map0);

    /* And check second map hasn't changed */
    for(int i = 0; i < 1024; i++)
        assert(map1[i] == (i ^ 7));

    /* CLeanup */
    vmm_transient_unmap(map1);
}

static void test_identity_mapped()
{
    trace("Testing kernel is properly identity-mapped");

    /* Test kernel is properly identity-mapped */
    for(unsigned char* page = (unsigned char*)KERNEL_START; 
        page < (unsigned char*)KERNEL_END; 
        page += PAGE_SIZE) {

        assert(is_identity_mapped(page));
    }
}

static void test_recursive_mapping()
{
    /*
     * Current pagedir available at 0xFFFFF000 and in cr3
     */
    uint32_t* pagedir0 = (uint32_t*)0xFFFFF000;
    uint32_t* pagedir1 = vmm_transient_map(read_cr3(), VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    for(int i = 0; i < 1024; i++) {
        if(pagedir0[i] != pagedir1[i]) {
            trace("i: %d, pagedir0[i]: %p, pagedir1[i]: %p",
                  i,
                  pagedir0[i],
                  pagedir1[i]);
            abort();
        }
    }
    vmm_transient_unmap(pagedir1);
}

static void test_map()
{
    trace("Testing vmm_map");

    /* Try mapping a new page at 32Mb */
    volatile uint32_t* my_page = (uint32_t*)(32 * 1024 * 1024);
    uint32_t page_frame = pmm_alloc();
    vmm_map((uint32_t*)my_page, page_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);

    /* read and write from it */
    uint32_t value = *my_page;
    value ^= 7;
    *my_page = value;

    /* Unmap it */
    vmm_unmap((uint32_t*)my_page);
    pmm_free(page_frame);
}

static void* unmap_env[20];

static void unmap_pf_handler(struct isr_regs* regs)
{
    __builtin_longjmp(unmap_env, 1);
    assert(!"longjmp failed");
}

static void test_unmap()
{
    trace("Testing vmm_unmap");

    /* Map a new page at 33Mb */
    volatile uint32_t* test_page = (uint32_t*)(33 * 1024 * 1024);
    uint32_t page_frame = pmm_alloc();
    vmm_map((uint32_t*)test_page, page_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);

    /* Test we can read and write from that page */
    uint32_t val = *test_page;
    val ^= 7;
    *test_page = val;

    /* Unmap that page */
    vmm_unmap((void*)test_page);
    pmm_free(page_frame);

    /* Install page fault handler so we can longjmp back */
    isr_handler_t old_handler = idt_get_handler(14);
    idt_install(14, unmap_pf_handler, false);
    int status = __builtin_setjmp(unmap_env);
    if(status) {
        idt_install(14, old_handler, false);
        return;
    }

    val = *test_page;
    val ^= 0xB00B5;
    *test_page = val;

    assert(!"No page fault!");
}

static void test_clone_pagedir()
{
    trace("Testing vmm_clone_pagedir");

    unsigned hi_kernel_space_start = 3U * 1024U * 1024U * 1024U;
    unsigned bytes_per_pde = 4U * 1024U * 1024U;
    unsigned char* userspace_start = (unsigned char*)(4U * 1024 * 1024);

    /* Clone a pagedir using current pagedir's state */
    trace("Clone current pagedir");
    struct pagedir* pagedir = vmm_clone_pagedir();

    /* 
     * This new pagedir should be a perfect clone of current pagedir
     * (except last entry and user-space)
     */
    uint32_t* current_pagedir = (uint32_t*)vmm_current_pagedir();
    uint32_t* new_pagedir = (uint32_t*)pagedir;
    trace("new_pagedir[0]: %p, current_pagedir[0]: %p",
          new_pagedir[0], current_pagedir[0]);
    assert(new_pagedir[0] == current_pagedir[0]);       /* first 4Mb */

    for(unsigned i = hi_kernel_space_start / bytes_per_pde; i < 1022; i++) {
        assert(new_pagedir[i] == current_pagedir[i]);
    }

    assert(new_pagedir[1023] != current_pagedir[1023]);
    assert(new_pagedir[1023] & 0x1);        /* PDE_PRESENT */
    assert(new_pagedir[1023] & (1 << 1));   /* PDE_WRITABLE */

    /*
     * Now, map a page at start of userspace
     * Fill it with predictable data
     */
    trace("Map page at start of userspace");
    unsigned* userspace_data = (unsigned*)userspace_start;
    uint32_t frame0 = pmm_alloc();
    vmm_map(userspace_start, 
            frame0, 
            VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    trace("Fill with data");
    for(unsigned i = 0; i < PAGE_SIZE / sizeof(unsigned); i ++) {
        userspace_data[i] = i ^ 7;
    }

    /*
     * Switch to new pagedir
     */
    trace("Switch to new pagedir");
    vmm_switch_pagedir(pagedir);

    /*
     * Start of userspace shouldn't be mapped
     */
    uint32_t frame_flags = vmm_get_flags(userspace_data);
    assert(!(frame_flags & VMM_PAGE_PRESENT));

    /*
     * Check new pagedir is properly recursive mapped
     */
    uint32_t* pagedir_view = (uint32_t*)0xFFFFF000;
    for(int i = 0; i < 1024; i++) {
        assert(new_pagedir[i] == pagedir_view[i]);
    }
    uint32_t* pagetable_view = (uint32_t*)(0xFFC00000 + PAGE_SIZE);

    /*
     * Map it to another page
     * Fill with another set of predictable data
     */
    trace("Map another page");
    uint32_t frame1 = pmm_alloc();
    vmm_map(userspace_start,
            frame1,
            VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    trace("Fill with data");
    for(unsigned i = 0; i < PAGE_SIZE / sizeof(unsigned); i++) {
        userspace_data[i] = i ^ ~7;
    }

    /*
     * Switch back to original pagedir
     */
    trace("Switch to original pagedir");
    vmm_switch_pagedir((struct pagedir*)current_pagedir);

    /*
     * Check data has not been changed
     */
    trace("Check data");
    for(unsigned i = 0; i < PAGE_SIZE / sizeof(unsigned); i++) {
        assert(userspace_data[i] != (i ^ ~7));
        assert(userspace_data[i] == (i ^ 7));
    } 

}

void test_vmm()
{
    trace("Testing vmm");

    /* Test kernel properly identity-mapped */
    test_identity_mapped();

    /* Test transient maps */
    test_transient_map();

    /* Test recursive mapping works correctly */
    test_recursive_mapping();

    /* Test vmm_map */
    test_map();

    /* Test vmm_unmap */
    test_unmap();

    /* testing pagedir cloning */
    test_clone_pagedir();
}





