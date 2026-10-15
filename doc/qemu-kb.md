# QEMU KB

## Get virtual address mapping

```
info mem
info tlb
gva2gpa 0xB8000
```

## Dump memory

- `x/<fmt> <address>` dump memory at virtual address
- `xp/<fmt> <address>` dump memory at physical address
- fmt: <count><format><size>
- format: x: hex, d: signed, u: unsigned, o: octal, c: char, i: instructions
- size: b (8 bits), h (16 bits), w (32 bits), g (64 bits)
- h or w can be combined with i to select instruction size

Examples:

```
xp/16i 0x7c00
x/16bx 0x100000
```

