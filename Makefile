.PHONY: all clean run kernel/obj/kernel.elf

all: kernel/obj/kernel.elf

PrototypeOS.iso: build/boot/grub build/boot/grub/grub.cfg build/kernel.elf
	@echo "[BLD] $@"
	@grub-mkrescue -o $@ build

build/boot/grub:
	@mkdir -p $@

build/boot/grub/grub.cfg: boot/grub.cfg
	@cp $^ $@

build/kernel.elf: kernel/obj/kernel.elf

kernel/obj/kernel.elf:
	@echo "[MK] kernel"
	@make --no-print-directory -C kernel

clean:
	@rm -rf build PrototypeOS.iso
	@make --no-print-directory -C kernel clean

run:
	@qemu-system-i386 -kernel kernel/obj/kernel.elf -no-reboot -nographic -debugcon file:/tmp/PrototypeOS.log -m 32




