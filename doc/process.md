# processes


## processes

- Decoupled from the concept of address spaces.
- Each process has its own address space
- There is a global process registry in process.c
- The initial process becomes kernel_task

## Address Spaces

- Implemented in address_space.c and address_space.h
- Manages virtual memory mappings
- Aware of user/kernel separation
- Pages in kernel area start at KERNEL_OFFSET
- Page directories in kernel area are shared between all address spaces
- Frames mapped in user space are freed through the pmm except when they have the VMM_SHARED flag set

## context switching

- The thread manager (kthread.c) is responsible for switching address spaces

