.PHONY: all clean run debug
.SUFFIXES:

AS := nasm
CC := i686-elf-gcc

ASFLAGS := -f elf32 -g
CFLAGS := -std=gnu99 -ffreestanding -Werror -g -march=i386 -mpreferred-stack-boundary=2
LDFLAGS := -g -ffreestanding -nostdlib -T kernel.ld

all: obj/kernel.elf

obj:
	mkdir -p obj

obj/kernel.elf: \
	obj/stub.asm.o \
	obj/idt.asm.o \
	obj/main.c.o \
	obj/gdt.c.o \
	obj/idt.c.o \
	obj/debug.c.o \
	obj/string.c.o
	$(CC) $(LDFLAGS) -o $@ $^

obj/main.c.o: main.c Makefile | obj
	$(CC) $(CFLAGS) -c -o $@ -MMD -MP $<

obj/gdt.c.o: gdt.c Makefile | obj
	$(CC) $(CFLAGS) -c -S -o $@.S $<
	$(CC) $(CFLAGS) -c -o $@ -MMD -MP $<

obj/idt.c.o: idt.c Makefile | obj
	$(CC) $(CFLAGS) -c -S -o $@.S $<
	$(CC) $(CFLAGS) -c -o $@ -MMD -MP $<

obj/debug.c.o: debug.c Makefile | obj
	$(CC) $(CFLAGS) -c -o $@ -MMD -MP $<

obj/string.c.o: string.c Makefile | obj
	$(CC) $(CFLAGS) -c -o $@ -MMD -MP $<

obj/stub.asm.o: stub.asm | obj
	$(AS) $(ASFLAGS) -o $@ $<

obj/idt.asm.o: idt.asm | obj
	$(AS) $(ASFLAGS) -o $@ $<

run: obj/kernel.elf
	# C-a x to exit qemu
	# C-a h for help
	qemu-system-i386 -nographic -no-reboot -kernel obj/kernel.elf

debug: obj/kernel.elf
	# make debug
	# gdb -tui obj/kernel.elf
	# target remote localhost:1234
	# b kmain
	# c
	qemu-system-i386 -nographic -no-reboot -kernel obj/kernel.elf -s -S

clean:
	rm -rf obj

-include obj/main.c.d

