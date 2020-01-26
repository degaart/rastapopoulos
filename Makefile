.PHONY: all clean run

all: PrototypeOS.iso

PrototypeOS.iso: build/boot/grub build/boot/grub/grub.cfg build/kernel.elf
	grub-mkrescue -o $@ build

build/boot/grub:
	mkdir -p $@

build/boot/grub/grub.cfg: boot/grub.cfg
	cp $^ $@

build/kernel.elf: kernel/obj/kernel.elf
	make -C kernel

run:
	qemu-system-i386 -kernel kernel/obj/kernel.elf




