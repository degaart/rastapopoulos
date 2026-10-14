# Asm Knowledge base

## Disassemble a specific function

```
i686-elf-objdump -D -Mintel --disassemble=_start build/kernel.elf
```

The -D tells objdump to disassemble the symbol even if it's in a non-text
section.


