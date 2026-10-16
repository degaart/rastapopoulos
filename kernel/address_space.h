#pragma once

#include "kernel.h"
#include <freebsd/queue.h>
#include <stdbool.h>
#include <stdint.h>

#define PAGE_PRESENT  (1 << 0)
#define PAGE_WRITABLE (1 << 1)
#define PAGE_USER     (1 << 2)

/*
 * WARNING: Forgetting to destroy an address space
 * may lead to memory corruption
 * Suppose address space is allocated on the stack.
 * It is added to the list of address spaces.
 * When its storage is deallocated on function exit,
 * it corrupts the address space list, resulting in
 * strange bugs!!!
 */
struct AddressSpace
{
    uint32_t* pagedir;
    uint32_t pagedir_phys;
    bool destroyable;
    TAILQ_ENTRY(AddressSpace) node;
};

void address_space_init(void);
bool address_space_create(struct AddressSpace* as) NODISCARD;
void address_space_destroy(struct AddressSpace* as);
void address_space_activate(struct AddressSpace* as);
struct AddressSpace* address_space_current(void);
bool address_space_map(struct AddressSpace* as, void* pa, void* va,
                       unsigned flags) NODISCARD;
bool address_space_unmap(struct AddressSpace* as, void* va) NODISCARD;
bool address_space_remap(struct AddressSpace* as, void* va,
                         unsigned flags) NODISCARD;

/*
 * Query physical address and flags of the specified virtual address
 * frame and flags can be NULL
 * Returns false if frame is unmapped or invalid. In that case, frame and flags
 * are untouched Returns true if frame is mapped frame and flags may be NULL
 */
bool address_space_query(struct AddressSpace* as, const void* va, void** frame,
                         unsigned* flags) NODISCARD;

