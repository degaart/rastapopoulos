.PHONY: all clean run
.SUFFIXES:

AS := nasm
CC := i686-elf-gcc

ASFLAGS := -f elf32
CFLAGS := -std=gnu99 -ffreestanding -Werror
LDFLAGS := -ffreestanding -nostdlib -T kernel.ld

all: obj/kernel.elf

obj:
	mkdir -p obj

obj/kernel.elf: obj/stub.asm.o obj/main.c.o
	$(CC) $(LDFLAGS) -o $@ $^

obj/main.c.o: main.c Makefile | obj
	$(CC) $(CFLAGS) -c -o $@ -MMD -MP $<

obj/stub.asm.o: stub.asm | obj
	$(AS) $(ASFLAGS) -o $@ $<

run: obj/kernel.elf
	# C-a x to exit qemu
	# C-a h for help
	qemu-system-i386 -nographic -no-reboot -kernel obj/kernel.elf

clean:
	rm -rf obj

-include obj/main.c.d

