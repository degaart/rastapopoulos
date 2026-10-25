#include "../address_space.h"
#include "../kernel.h"
#include "../kmalloc.h"
#include "../pmm.h"
#include "../trace.h"
#include <stddef.h>
#include <stdint.h>

#define FRAME_MASK       0xfffff000
#define RECURSIVE_INDEX  1023
#define RECURSIVE_BASE   0xffc00000
#define USER_ADDRESS     ((void*)0x40000000)
#define INACTIVE_ADDRESS ((void*)0x40400000)

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition))                                                     \
            panic("Address-space test failed: %s", #condition);               \
    } while (0)

/* Set to 1 or 2 for separate QEMU runs that must panic, never report success.
 */
#ifndef ADDRESS_SPACE_NEGATIVE_CASE
#    define ADDRESS_SPACE_NEGATIVE_CASE 0
#endif

static struct AddressSpace allocation_spaces[1024];
static size_t checks;

static void passed(char const* name)
{
    checks++;
    trace("Address-space check %zu: %s", checks, name);
}

static void* alloc_frame(void)
{
    void* frame = pmm_alloc();
    CHECK(frame != NULL);
    CHECK(IS_ALIGNED(frame, PAGE_SIZE));
    return frame;
}

static void activate(struct AddressSpace* as)
{
    address_space_activate(as);
    CHECK(address_space_current() == as);
    CHECK(read_cr3() == as->pagedir_phys);
    CHECK(!(read_eflags() & EFLAGS_IF));
}

static uint32_t pte(struct AddressSpace* as, void* va)
{
    struct AddressSpace* previous = address_space_current();
    size_t index = (uintptr_t)va >> 22;
    CHECK(as->pagedir[index] & PAGE_PRESENT);
    activate(as);
    volatile uint32_t* table =
        (volatile uint32_t*)(RECURSIVE_BASE + index * PAGE_SIZE);
    uint32_t entry = table[((uintptr_t)va >> 12) & 1023];
    activate(previous);
    return entry;
}

static void mapping(struct AddressSpace* as, void* va, void* frame,
                    unsigned permissions)
{
    void* actual = NULL;
    unsigned flags = 0;
    CHECK(address_space_query(as, va, &actual, &flags));
    CHECK(actual == frame);
    CHECK((flags & (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)) ==
          (permissions | PAGE_PRESENT));
    CHECK((pte(as, va) & FRAME_MASK) == (uintptr_t)frame);
    CHECK((pte(as, va) & 7) == (permissions | PAGE_PRESENT));
    uint32_t directory = as->pagedir[(uintptr_t)va >> 22];
    CHECK(directory & PAGE_PRESENT);
    CHECK(directory & PAGE_WRITABLE);
    CHECK(!!(directory & PAGE_USER) == ((uintptr_t)va < KERNEL_OFFSET));
}

static void absent(struct AddressSpace* as, void* va)
{
    void* frame = (void*)0x12345000;
    unsigned flags = 0x1234;
    CHECK(!address_space_query(as, va, &frame, &flags));
    CHECK(frame == (void*)0x12345000);
    CHECK(flags == 0x1234);
    if (as->pagedir[(uintptr_t)va >> 22] & PAGE_PRESENT)
        CHECK(!(pte(as, va) & PAGE_PRESENT));
}

static void directory(struct AddressSpace* as, struct AddressSpace* original)
{
    CHECK(IS_ALIGNED(as->pagedir, PAGE_SIZE));
    CHECK(IS_ALIGNED(as->pagedir_phys, PAGE_SIZE));
    CHECK(as->pagedir_phys != original->pagedir_phys);
    CHECK((as->pagedir[RECURSIVE_INDEX] & (FRAME_MASK | 7)) ==
          (as->pagedir_phys | PAGE_PRESENT | PAGE_WRITABLE));
    for (size_t i = KERNEL_OFFSET >> 22; i < RECURSIVE_INDEX; i++)
        CHECK((as->pagedir[i] & (FRAME_MASK | 7)) ==
              (original->pagedir[i] & (FRAME_MASK | 7)));
    absent(as, NULL);
}

static void contents(struct AddressSpace* as, void* va, uint32_t value)
{
    activate(as);
    CHECK(*(volatile uint32_t*)va == value);
}

static void functional(struct AddressSpace* original)
{
    struct AddressSpace a, b, late;
    CHECK(address_space_create(&a));
    CHECK(address_space_create(&b));
    directory(&a, original);
    directory(&b, original);
    CHECK(a.pagedir_phys != b.pagedir_phys);
    passed("directory creation, recursive permissions and shared kernel PDEs");

    void* fa = alloc_frame();
    void* fb = alloc_frame();
    CHECK(fa != fb);
    CHECK(address_space_map(&a, fa, USER_ADDRESS, PAGE_USER | PAGE_WRITABLE));
    CHECK(address_space_map(&b, fb, USER_ADDRESS, PAGE_USER | PAGE_WRITABLE));
    mapping(&a, USER_ADDRESS, fa, PAGE_USER | PAGE_WRITABLE);
    mapping(&b, USER_ADDRESS, fb, PAGE_USER | PAGE_WRITABLE);
    activate(&a);
    *(volatile uint32_t*)USER_ADDRESS = 0xaabbccdd;
    activate(&b);
    *(volatile uint32_t*)USER_ADDRESS = 0x11223344;
    for (unsigned i = 0; i < 16; i++) {
        contents(&a, USER_ADDRESS, 0xaabbccdd);
        contents(&b, USER_ADDRESS, 0x11223344);
    }
    passed("user isolation and CR3 activation");

    size_t before = pmm_info();
    CHECK(!address_space_map(&a, fb, USER_ADDRESS, PAGE_USER));
    CHECK(pmm_info() == before);
    mapping(&a, USER_ADDRESS, fa, PAGE_USER | PAGE_WRITABLE);
    contents(&a, USER_ADDRESS, 0xaabbccdd);
    CHECK(address_space_remap(&b, USER_ADDRESS, PAGE_USER));
    mapping(&b, USER_ADDRESS, fb, PAGE_USER);
    CHECK(address_space_remap(&b, USER_ADDRESS, PAGE_USER | PAGE_WRITABLE));
    passed("duplicate rejection preserves ownership; inactive permission "
           "changes");

    void* inactive = alloc_frame();
    activate(&a);
    before = pmm_info();
    CHECK(address_space_map(&b, inactive, INACTIVE_ADDRESS,
                            PAGE_USER | PAGE_WRITABLE));
    CHECK(pmm_info() + PAGE_SIZE == before);
    absent(&a, INACTIVE_ADDRESS);
    mapping(&b, INACTIVE_ADDRESS, inactive, PAGE_USER | PAGE_WRITABLE);
    activate(&b);
    *(volatile uint32_t*)INACTIVE_ADDRESS = 0x98765432;
    contents(&b, INACTIVE_ADDRESS, 0x98765432);
    activate(&a);
    CHECK(address_space_unmap(&b, INACTIVE_ADDRESS));
    CHECK(pmm_info() == before);
    absent(&b, INACTIVE_ADDRESS);
    CHECK(b.pagedir[(uintptr_t)INACTIVE_ADDRESS >> 22] == 0);
    pmm_free(inactive);
    CHECK(!address_space_unmap(&b, INACTIVE_ADDRESS));
    passed("inactive map/query/unmap and caller-owned backing frame");

    void* existing = NULL;
    size_t kernel_index = (uintptr_t)__kernel_start >> 22;
    for (size_t i = 1023; i > 0; i--) {
        void* candidate = (void*)((kernel_index << 22) + i * PAGE_SIZE);
        if (!address_space_query(original, candidate, NULL, NULL)) {
            existing = candidate;
            break;
        }
    }
    CHECK(existing != NULL);
    void* shared = alloc_frame();
    CHECK(address_space_map(&a, shared, existing, PAGE_WRITABLE));
    activate(&a);
    *(volatile uint32_t*)existing = 0xfeedbeef;
    struct AddressSpace* spaces[] = {original, &a, &b};
    for (size_t i = 0; i < 3; i++) {
        mapping(spaces[i], existing, shared, PAGE_WRITABLE);
        contents(spaces[i], existing, 0xfeedbeef);
    }
    passed("mapping through an existing shared kernel table");

    size_t dedicated_index = 1022;
    while (dedicated_index > (KERNEL_OFFSET >> 22) &&
           original->pagedir[dedicated_index] != 0)
        dedicated_index--;
    CHECK(dedicated_index > (KERNEL_OFFSET >> 22));
    void* dedicated = (void*)(dedicated_index << 22);
    void* published = alloc_frame();
    before = pmm_info();
    CHECK(address_space_map(&b, published, dedicated, PAGE_WRITABLE));
    CHECK(pmm_info() + PAGE_SIZE == before);
    activate(&b);
    *(volatile uint32_t*)dedicated = 0xcafebabe;
    for (size_t i = 0; i < 3; i++) {
        CHECK((spaces[i]->pagedir[dedicated_index] & (FRAME_MASK | 7)) ==
              (b.pagedir[dedicated_index] & (FRAME_MASK | 7)));
        mapping(spaces[i], dedicated, published, PAGE_WRITABLE);
        contents(spaces[i], dedicated, 0xcafebabe);
    }
    CHECK(address_space_create(&late));
    mapping(&late, dedicated, published, PAGE_WRITABLE);
    contents(&late, dedicated, 0xcafebabe);
    activate(original);
    address_space_destroy(&late);
    passed("new kernel table publication and inheritance");

    contents(&a, dedicated, 0xcafebabe);
    (void)pte(&a, dedicated);
    before = pmm_info();
    CHECK(address_space_unmap(&b, dedicated));
    CHECK(pmm_info() == before + PAGE_SIZE);
    for (size_t i = 0; i < 3; i++) {
        CHECK(spaces[i]->pagedir[dedicated_index] == 0);
        absent(spaces[i], dedicated);
    }
    volatile uint32_t* recursive_directory = (volatile uint32_t*)0xfffff000u;
    CHECK(recursive_directory[dedicated_index] == 0);
    CHECK(!address_space_unmap(&a, dedicated));
    pmm_free(published);
    passed("shared table removal and single table-frame reclamation");

    activate(original);
    CHECK(address_space_unmap(original, existing));
    pmm_free(shared);
    CHECK(address_space_unmap(&a, USER_ADDRESS));
    CHECK(address_space_unmap(&b, USER_ADDRESS));
    pmm_free(fa);
    pmm_free(fb);
    address_space_destroy(&a);
    address_space_destroy(&b);
    CHECK(
        address_space_map(original, (void*)0xb8000, dedicated, PAGE_WRITABLE));
    mapping(original, dedicated, (void*)0xb8000, PAGE_WRITABLE);
    CHECK(address_space_unmap(original, dedicated));
    CHECK(!pmm_reserve((void*)0xb8000));
    passed("registry removal, surviving scratch access and borrowed VGA "
           "ownership");

    CHECK(!address_space_map(original, (void*)1, dedicated, PAGE_WRITABLE));
    CHECK(!address_space_map(original, NULL, NULL, PAGE_WRITABLE));
    CHECK(!address_space_map(original, NULL, (void*)RECURSIVE_BASE,
                             PAGE_WRITABLE));
    CHECK(!address_space_map(original, NULL, (void*)((uintptr_t)dedicated + 1),
                             0));
    CHECK(!address_space_map(original, NULL, dedicated, FRAME_MASK));
    CHECK(!address_space_remap(original, dedicated, PAGE_WRITABLE));
    absent(original, NULL);
    passed("invalid addresses, null page and reserved recursive window");
}

static void lifecycle(struct AddressSpace* original)
{
    for (unsigned iteration = 0; iteration < 9; iteration++) {
        size_t before = pmm_info();
        struct AddressSpace as;
        CHECK(address_space_create(&as));
        void* frame = alloc_frame();
        CHECK(address_space_map(&as, frame, USER_ADDRESS,
                                PAGE_USER | PAGE_WRITABLE));
        activate(&as);
        *(volatile uint32_t*)USER_ADDRESS = iteration;
        contents(&as, USER_ADDRESS, iteration);
        activate(original);
        address_space_destroy(&as);
        CHECK(!pmm_reserve(frame));
        pmm_free(frame);
        if (iteration != 0)
            CHECK(pmm_info() == before);
    }
    passed("repeated destruction reclaims tables and directory storage; "
           "caller frees private frames");
}

static void allocation_failures(struct AddressSpace* original)
{
    struct AddressSpace as;
    CHECK(address_space_create(&as));
    size_t count = pmm_info() / PAGE_SIZE;
    void** held = kmalloc(count * sizeof(*held));
    CHECK(held != NULL);
    size_t used = 0;
    void* frame;
    while ((frame = pmm_alloc()) != NULL) {
        CHECK(used < count);
        held[used++] = frame;
    }
    CHECK(pmm_info() == 0);
    CHECK(used != 0);
    CHECK(!address_space_map(&as, held[0], USER_ADDRESS, PAGE_USER));
    CHECK(as.pagedir[(uintptr_t)USER_ADDRESS >> 22] == 0);
    absent(&as, USER_ADDRESS);
    size_t index = 1022;
    while (index > (KERNEL_OFFSET >> 22) && original->pagedir[index])
        index--;
    CHECK(index > (KERNEL_OFFSET >> 22));
    CHECK(
        !address_space_map(&as, held[0], (void*)(index << 22), PAGE_WRITABLE));
    CHECK(as.pagedir[index] == 0);
    CHECK(original->pagedir[index] == 0);
    CHECK(pmm_info() == 0);
    passed(
        "private and shared table allocation failure rolls back publication");

    size_t created = 0;
    while (created <
               sizeof(allocation_spaces) / sizeof(allocation_spaces[0]) &&
           address_space_create(&allocation_spaces[created]))
        created++;
    CHECK(created < sizeof(allocation_spaces) / sizeof(allocation_spaces[0]));
    CHECK(allocation_spaces[created].pagedir == NULL);
    CHECK(pmm_info() == 0);
    while (created)
        address_space_destroy(&allocation_spaces[--created]);
    for (size_t i = 0; i < used; i++)
        pmm_free(held[i]);
    kfree(held);
    address_space_destroy(&as);
    CHECK(address_space_create(&as));
    CHECK(address_space_map(&as, (void*)0xb8000, (void*)(index << 22),
                            PAGE_WRITABLE));
    CHECK(original->pagedir[index] == as.pagedir[index]);
    CHECK(address_space_unmap(original, (void*)(index << 22)));
    address_space_destroy(&as);
    passed("directory allocation failure leaves no stale registry member");
}

void test_address_spaces(void)
{
    trace("Running address-space tests");
    bool interrupts = enter_critical_section();
    struct AddressSpace* original = address_space_current();
    CHECK(original != NULL);
    CHECK(read_cr3() == original->pagedir_phys);
    if (ADDRESS_SPACE_NEGATIVE_CASE == 1) {
        struct AddressSpace as;
        CHECK(address_space_create(&as));
        activate(&as);
        trace("Expect panic: destroying the active address space");
        address_space_destroy(&as);
        panic("Active address-space destruction was accepted");
    }
    if (ADDRESS_SPACE_NEGATIVE_CASE == 2) {
        trace("Expect panic: destroying the permanent original address space");
        address_space_destroy(original);
        panic("Permanent address-space destruction was accepted");
    }
    functional(original);
    allocation_failures(original);
    lifecycle(original);
    activate(original);
    leave_critical_section(interrupts);
    trace("All %zu address-space checks succeeded", checks);
    debugexit(1);
}
