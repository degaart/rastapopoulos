# Roadmap
- print to qemu debug port [OK]
- implement panic [OK]
- implement stack smashing protection [OK]

# Implementation details
- I/O port access functions in kernel/io.h. Also bochs debug port defined there

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




