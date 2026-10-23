#include "vmm.h"
#include "address_space.h"
#include "early_malloc.h"

bool vmm_init(void)
{
    early_malloc_disable();
    return true;
}

bool vmm_map(void* frame, void* virtual_address, uint32_t flags)
{
    struct AddressSpace* as = address_space_current();
    if (!as)
        return false;
    return address_space_map(as, frame, virtual_address, flags);
}

bool vmm_unmap(void* virtual_address)
{
    struct AddressSpace* as = address_space_current();
    if (!as)
        return false;
    return address_space_unmap(as, virtual_address);
}

bool vmm_remap(void* virtual_address, uint32_t flags)
{
    struct AddressSpace* as = address_space_current();
    if (!as)
        return false;
    return address_space_remap(as, virtual_address, flags);
}

bool vmm_frame(const void* virtual_address, void** frame_out)
{
    struct AddressSpace* as = address_space_current();
    if (!as)
        return false;
    void* vaddr = (void*)((uintptr_t)virtual_address & -PAGE_SIZE);
    return address_space_query(as, vaddr, frame_out, NULL);
}

bool vmm_is_valid_userptr(const void* ptr, size_t len, bool for_write)
{
    return address_space_is_valid_userptr(ptr, len, for_write);
}

bool vmm_is_valid_userstr(const char* str)
{
    return address_space_is_valid_userstr(str);
}

