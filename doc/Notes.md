# VGA
- 80x25 mode 6 VGA buffer: 0xB8000
    - format: (ch & 0xFF) | (color << 8)
        - color: (fg & 0xF)|((bf & 0xF) << 4)

# sysv ABI
- stack must be aligned to 16 bytes
- args are pushed on the stack from right to left

# grub
- check if file is multiboot compliant: ```grub-file --is-x86-multiboot myos.bin```
- check for multiboot2: ```--is-x86-multiboot2```

# qemu
- can boot multiboot kernel with -kernel

# assembler
- can mark a symbol as a function with: global _start:function (_start.end - _start)




