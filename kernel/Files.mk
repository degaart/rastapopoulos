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
    pmm_memregion.cpp

ASM_SRCS = \
    stub.asm \
    outb.asm \
    inb.asm \
    call_ctors.asm \
    gdt_flush.asm \
    idt_stub.asm \
    idt_flush.asm
