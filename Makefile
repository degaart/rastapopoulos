.PHONY: all clean run run_graphic kernel/obj/kernel.elf build/initrd.tar

all: kernel/obj/kernel.elf build/initrd.tar

PrototypeOS.iso: build/boot/grub build/boot/grub/grub.cfg build/boot/kernel.elf kernel/obj/kernel.elf build/initrd.tar
	@echo "[BLD] $@"
	@grub-mkrescue -o $@ build

build/boot/grub:
	@mkdir -p $@

build/boot/grub/grub.cfg: boot/grub.cfg
	@cp $^ $@

build/boot/kernel.elf: kernel/obj/kernel.elf
	@cp $^ $@

build/initrd.tar:
	@echo "[TAR] initrd.tar"
	@make --no-print-directory -C userland
	@cp userland/hello/obj/hello.elf build/
	@tar cf build/initrd.tar -C build hello.elf message.txt

kernel/obj/kernel.elf:
	@echo "[MK] kernel"
	@make --no-print-directory -C kernel

clean:
	@rm -rf build PrototypeOS.iso
	@make --no-print-directory -C kernel clean

run:
	@qemu-system-i386 \
		-kernel kernel/obj/kernel.elf \
		-append "Hello, world!" \
		-initrd build/initrd.tar \
		-no-reboot \
		-nographic \
		-debugcon file:/tmp/PrototypeOS.log \
		-m 8 \
		-monitor telnet:127.0.0.1:55555,server,nowait

run_graphic:
	@qemu-system-i386 \
		-kernel kernel/obj/kernel.elf \
		-initrd build/initrd.tar \
		-no-reboot \
		-debugcon file:/tmp/PrototypeOS.log \
		-m 8 \
		-monitor telnet:127.0.0.1:55555,server,nowait

debug:
	@qemu-system-i386 \
		-kernel kernel/obj/kernel.elf \
		-initrd build/initrd.tar \
		-no-reboot \
		-debugcon file:/tmp/PrototypeOS.log \
		-m 8 \
		-s -S



