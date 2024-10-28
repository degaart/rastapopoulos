.PHONY: all run clean

all:
	make -C tools
	make -C user
	make -C kernel
	make -C bootloader

run: all
	qemu-system-i386 \
		-m 16 \
		-drive file=bootloader/obj/boot.img,if=floppy,format=raw,readonly=on \
		-chardev stdio,id=char0 \
		-serial chardev:char0 \
		-vga cirrus

debug: all
	qemu-system-i386 \
		-m 16 \
		-drive file=bootloader/obj/boot.img,if=floppy,format=raw,readonly=on \
		-chardev stdio,id=char0 \
		-serial chardev:char0 \
		-vga cirrus \
		-s -S

clean:
	make -C tools clean
	make -C user clean
	make -C kernel clean
	make -C bootloader clean
