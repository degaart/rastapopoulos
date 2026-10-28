#include "../address_space.h"
#include "../kmalloc.h"
#include "../pmm.h"
#include "../vmm.h"
#include "test.h"

static void bounded_allocations(void)
{
    unsigned capacity = 0x1000000 / PAGE_SIZE;
    void** held = kmalloc(capacity * sizeof(*held));
    CHECK(held);
    enter_critical_section();
    size_t before = pmm_info();
    CHECK(!pmm_alloc_below(0));
    CHECK(!pmm_alloc_below(PAGE_SIZE - 1));
    CHECK(!pmm_alloc_below(PAGE_SIZE));
    CHECK(pmm_info() == before);

    void* first = pmm_alloc();
    CHECK(first);
    CHECK(IS_ALIGNED(first, PAGE_SIZE));
    uintptr_t end = (uintptr_t)first + PAGE_SIZE;
    pmm_free(first);
    CHECK(!pmm_alloc_below(end - 1));
    CHECK(pmm_info() == before);
    CHECK(pmm_alloc_below(end) == first);
    CHECK(pmm_info() == before - PAGE_SIZE);
    pmm_free(first);
    CHECK(pmm_alloc_below(end + 1) == first);
    pmm_free(first);
    CHECK(pmm_alloc_below(UINTPTR_MAX) == first);
    pmm_free(first);

    unsigned count = 0;
    void* frame;
    while ((frame = pmm_alloc_below(0x1000000)) != NULL) {
        CHECK(count < capacity);
        CHECK(IS_ALIGNED(frame, PAGE_SIZE));
        CHECK((uintptr_t)frame <= 0x1000000 - PAGE_SIZE);
        held[count++] = frame;
    }
    CHECK(count > 0);
    CHECK(pmm_info() == before - count * PAGE_SIZE);
    CHECK(!pmm_alloc_below(0x1000000));
    CHECK(pmm_info() == before - count * PAGE_SIZE);
    frame = pmm_alloc();
    CHECK(frame && (uintptr_t)frame >= 0x1000000);
    pmm_free(frame);
    for (unsigned i = 0; i < count; i++)
        pmm_free(held[i]);
    CHECK(pmm_info() == before);
    CHECK(pmm_alloc_below(end) == first);
    pmm_free(first);
    leave_critical_section();
    kfree(held);
}

static void kernel_mappings(void)
{
    void* first = pmm_alloc_below(0x1000000);
    void* second = pmm_alloc_below(0x1000000);
    CHECK(first && second && first != second);
    size_t before = pmm_info();
    CHECK(!vmm_map_kernel((void*)1, VMM_WRITABLE));
    CHECK(!vmm_map_kernel(first, VMM_USER));
    CHECK(!vmm_map_kernel(first, PAGE_SIZE));
    CHECK(pmm_info() == before);

    uint32_t* a = vmm_map_kernel(first, VMM_WRITABLE);
    uint32_t* b = vmm_map_kernel(second, VMM_WRITABLE);
    CHECK(a && b && a != b);
    CHECK((uintptr_t)a >= KERNEL_OFFSET && (uintptr_t)a < 0xffc00000);
    CHECK((uintptr_t)b >= KERNEL_OFFSET && (uintptr_t)b < 0xffc00000);
    a[0] = 0x12345678;
    b[0] = 0x87654321;
    a[PAGE_SIZE / sizeof(*a) - 1] = 0xabcdef01;
    CHECK(a[0] == 0x12345678 && b[0] == 0x87654321);
    CHECK(a[PAGE_SIZE / sizeof(*a) - 1] == 0xabcdef01);
    void* physical;
    CHECK(vmm_frame(a, &physical) && physical == first);
    CHECK(vmm_frame(b, &physical) && physical == second);
    struct AddressSpace child;
    CHECK(address_space_create(&child));
    unsigned flags;
    CHECK(address_space_query(&child, a, &physical, &flags));
    CHECK(physical == first && (flags & PAGE_WRITABLE) &&
          !(flags & PAGE_USER));
    CHECK(address_space_query(&child, b, &physical, NULL));
    CHECK(physical == second);
    address_space_destroy(&child);
    CHECK(vmm_unmap(a));
    CHECK(vmm_unmap(b));
    CHECK(!vmm_frame(a, &physical));
    CHECK(!vmm_frame(b, &physical));
    CHECK(pmm_reserve(first));
    CHECK(pmm_reserve(second));
    pmm_free(first);
    pmm_free(second);
}

void test_pmm(void)
{
    trace("*** PMM tests started");
    bounded_allocations();
    kernel_mappings();
    trace("*** PMM tests done");
}
