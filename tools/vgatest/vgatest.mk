.SUFFIXES:
.PHONY: all clean

all: obj/vgatest.img

obj/vgatest.img: obj/vgatest.bin
	@ echo "[DD] $@"
	@ dd if=/dev/zero of=obj/vgatest.img bs=512 count=2880
	@ dd if=obj/vgatest.bin of=obj/vgatest.img bs=512 count=1 conv=notrunc

obj/vgatest.bin: vgatest.asm
	@ echo "[AS] $@"
	@ nasm -f bin -o $@ $^

run: obj/vgatest.img
	@ qemu-system-i386 -fda obj/vgatest.img -boot a -debugcon file:/tmp/rastapopoulos.log -m 8

clean:
	@ rm -rvf obj/*


