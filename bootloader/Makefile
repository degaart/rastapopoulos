.PHONY: all clean

all: boot.img

boot.img: boot0.bin boot1.bin make_bootsect
	mformat -C -f 1440 -i $@
	mcopy -i boot.img -Do $^ ::/
	./make_bootsect boot0.bin boot.img

boot0.bin: boot0.asm
	nasm -o $@ -f bin $<

boot1.bin: boot1.asm
	nasm -o $@ -f bin $<

make_bootsect: make_bootsect.c
	cc -o $@ $<

clean:
	rm -f *.o *.bin make_bootsect fatread

run: boot.img
	qemu-system-i386 -fda boot.img

fatread: fatread.c
	cc -o $@ $< -g -fno-omit-frame-pointer -fsanitize=address


