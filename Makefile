.PHONY: all run clean

all:
	make -C tools
	make -C user
	make -C kernel
	make -C bootloader

run: all
	qemu-system-i386 \
		-drive file=bootloader/obj/boot.img,if=floppy,format=raw,readonly=on \
		-chardev stdio,id=char0 \
		-serial chardev:char0

clean:
	make -C tools clean
	make -C user clean
	make -C kernel clean
	make -C bootloader clean
