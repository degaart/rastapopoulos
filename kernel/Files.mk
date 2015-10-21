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
    timer.cpp

ASM_SRCS = \
    stub.asm \
    outb.asm \
    inb.asm \
    call_ctors.asm \
    gdt_flush.asm \
    idt_stub.asm \
    idt_flush.asm \
    flush_tlb.asm \
    switch_to_usermode.asm \
    usermode_program.asm \
    resume_from_interrupt.asm \
    read_eip.asm
    
