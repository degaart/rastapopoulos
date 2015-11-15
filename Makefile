.PHONY: all clean kernel initrd

all: usb.img

debug: usb.img
	@bochs -q -f bochsrc -rc bochs.init

debug_graphic: usb.img
	@bochs -q -f bochsrc_graphic -rc bochs.init

run: usb.img
	@qemu-system-i386 -drive file=usb.img,format=raw -boot c -m 128 -debugcon file:/tmp/rastapopoulos.log -nographic -no-reboot

gdb: usb.img
	@qemu-system-i386 -drive file=usb.img,format=raw -boot c -m 128 -debugcon file:/tmp/rastapopoulos.log -nographic -s -S -no-reboot

run_graphic: usb.img
	@qemu-system-i386 -drive file=usb.img,format=raw -boot c -m 128 -debugcon file:/tmp/rastapopoulos.log -no-reboot -vga std

usb.img: kernel initrd
	@if ! [ -f usb.img ]; then echo "[INIT] $@"; ./mkimage.sh usb.img Rasta 64M; fi
	
	@echo "[COPY] kernel.elf"
	@./copyfile.sh usb.img kernel/obj/kernel.elf L:/

	@echo "[COPY] kernel.sym"
	@./copyfile.sh usb.img kernel/obj/kernel.sym L:/	

	@echo "[COPY] initrd.img"
	@./copyfile.sh usb.img initrd.img L:/

	@echo "[COPY] grub.cfg"
	@./copyfile.sh usb.img grub.cfg L:/boot/grub


# Problem: we can't specify kernel dependencies,
# so this makefile can't know when the kernel is
# outdated. So we must make the kernel a phony target
kernel:
	@echo "[MAKE] $@"
	@make -C kernel
	@make -C gensyms
	@gensyms/obj/gensyms kernel/obj/kernel.elf kernel/obj/kernel.sym

initrd:
	@make -C mkinitrd
	@make -C userland
	@echo "[INIT] $@"
	@mkinitrd/obj/mkinitrd initrd.img userland/hello/obj/hello.bin userland/vga/obj/vgadrv.bin

clean:
	@make -C kernel clean
	@make -C userland clean
	@make -C mkinitrd clean
	@rm -f *.o *.bin *.tmp *.img

