# Roadmap
- print to qemu debug port [OK]
- implement panic [OK]
- implement stack smashing protection [OK]
- gdt [OK]
- idt [OK]
- page frame allocator [ok]
- paging [OK]
    - vmm
    - integration between pmm and vmm
- higher-half [ok]

# Implementation details
- I/O port access functions in kernel/io.h. Also bochs debug port defined there
- why we need to remap pic in idt\_init: https://arjunsreedharan.org/post/99370248137/kernels-201-lets-write-a-kernel-with-keyboard
- Virtual memory map

```
    0x00000000 - 0x0009EFFF     Conventional memory
    0x0009F000 - 0x00100000     Reserved bios area
    0x00100000 - 0x003FFFFF     Kernel area (4mb)
    0x00400000 - 0xFF7FFFFF     User area
    0xFF800000 - 0xFFBFFFFF     Temporary mappings
    0xFFC00000 - 0xFFFFFFFF     Recursive PDE
```


# VGA
- 80x25 mode 6 VGA buffer: 0xB8000
    - format: (ch & 0xFF) | (color << 8)
        - color: (fg & 0xF)|((bf & 0xF) << 4)

# sysv ABI
- stack must be aligned to 16 bytes
- args are pushed on the stack from right to left
- 32-bit code: int is 32-bit, long is 32-bit, long long is 64-bit
- 64-bit code: int is 32-bit, long is 64-bit, long long is 64-bit

# grub
- check if file is multiboot compliant: ```grub-file --is-x86-multiboot myos.bin```
- check for multiboot2: ```--is-x86-multiboot2```
- grub will clear the .bss section for us

# qemu
- can boot multiboot kernel with -kernel
- exit nographic: C-a x

# assembler
- can mark a symbol as a function with: global _start:function (_start.end - _start)

# debugger
- ```--disable-debug --disable-dependency-tracking --target=i686-elf --without-python --disable-binutils MAKEINFO=false```

# bochs
- ```--disable-static --enable-debugger --enable-disasm --enable-debugger-gui --enable-readline --enable-x86-debugger --enable-clgd54xx --with-nogui```





