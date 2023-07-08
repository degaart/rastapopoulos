.PHONY: all clean run debug bochs disasm disasm1

CFLAGS = -masm=intel -Os -std=gnu99 -ffreestanding -Werror -g -march=i386 \
		 -mpreferred-stack-boundary=2 -fno-delete-null-pointer-checks \
		 -fno-finite-loops -fno-strict-aliasing
LDFLAGS = -Tboot1.ld -nostdlib

all: boot.img

boot.img: boot0.bin boot1.bin make_bootsect ../obj/kernel.elf
	mformat -C -f 1440 -i $@
	mcopy -i boot.img -Do boot0.bin boot1.bin ../obj/kernel.elf ::/
	./make_bootsect boot0.bin boot.img

boot0.bin: boot0.asm
	nasm -o $@ -f bin $<

make_bootsect: make_bootsect.c
	cc -o $@ $<

clean:
	rm -rf *.o *.S *.bin *.log *.img *.dSYM make_bootsect fatread crc32 t001 t002

run: boot.img
	qemu-system-i386 -drive file=boot.img,if=floppy,format=raw -boot a -no-reboot -serial stdio
	#qemu-system-i386 -drive file=boot.img,if=floppy,format=raw -boot a -no-reboot -nographic -d int

debug: boot.img
	# gdb commands
	# set architecture i8086
	# target remote localhost:1234
	# b *0x7C00
	# tui layout asm
	# tui layout split
	qemu-system-i386 -drive file=boot.img,if=floppy,format=raw -boot a -s -S -nographic -no-reboot

bochs: boot.img
	bochs -f bochsrc -rc bochsinit

fatread: fatread.c
	cc -o $@ $< -g -fno-omit-frame-pointer -fsanitize=address

disasm: boot0.bin
	ndisasm -b16 -k 3,59 boot0.bin

# boot1.o must be the first dependency
boot1.bin: boot1.o boot1.c.o
	i686-elf-gcc -o $@ $(LDFLAGS) $^

boot1.o: boot1.asm
	nasm -o $@ -f elf32 $<

boot1.c.o: boot1.c
	i686-elf-gcc -c -S -o $@.S $(CFLAGS) $<
	i686-elf-gcc -c -o $@ $(CFLAGS) $<

disasm1: boot1.c.o
	i686-elf-objdump -Mi8086,intel -S $^

crc32: crc32.c
	cc -o $@ $< -g

t001: t001.c
	cc -o $@ $< -g -fno-omit-frame-pointer -fsanitize=address

t002: t002.c
	cc -o $@ $< -g -fno-omit-frame-pointer -fsanitize=address

