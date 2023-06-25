.PHONY: all clean run debug bochs disasm

all: boot.img

boot.img: boot0.bin boot1.bin make_bootsect
	mformat -C -f 1440 -i $@
	mcopy -i boot.img -Do boot0.bin boot1.bin ::/
	./make_bootsect boot0.bin boot.img

boot0.bin: boot0.asm
	nasm -o $@ -f bin $<

boot1.bin: boot1.asm
	nasm -o $@ -f bin $<

make_bootsect: make_bootsect.c
	cc -o $@ $<

clean:
	rm -f *.o *.bin *.log *.img make_bootsect fatread

run: boot.img
	qemu-system-i386 -drive file=boot.img,if=floppy,format=raw -boot a -no-reboot

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

