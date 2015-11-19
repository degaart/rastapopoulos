SRCS = \
    main.cpp \
    cxxrt.cpp \
    string.cpp \
    debug.cpp \
    io.cpp \
    pmm.cpp \
    vmm.cpp \
    gdt.cpp \
    idt.cpp \
    kmalloc.cpp \
    bitset.cpp \
    pmm_memregion.cpp \
    heap.cpp \
    heap_block.cpp \
    kheap.cpp \
    util.cpp \
    pic.cpp \
    pit.cpp \
    pagedir.cpp \
    process.cpp \
    timer.cpp \
    syscall.cpp \
    initrd.cpp \
    port.cpp \
    backtrace.cpp \
    test_backtrace.cpp \
    test_vmm.cpp

C_SRCS = \
    elf.c

ASM_SRCS = \
    stub.asm \
    gdt_flush.asm \
    idt_stub.asm \
    idt_flush.asm \
    switch_to_usermode.asm \
    usermode_program.asm \
    resume_from_interrupt.asm \
    read_eip.asm
    
