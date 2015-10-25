.PHONY: all clean debug run gdb run_graphic bootsect bootldr kernel initrd.img

all: floppy.img

debug: floppy.img
	@/opt/bochs/bin/bochs -q -f bochsrc -rc bochs.init

run: floppy.img
	@qemu-system-i386 -drive file=floppy.img,if=floppy,format=raw -boot a -m 128 -debugcon file:/tmp/rastapopoulos.log -nographic -no-reboot

gdb: floppy.img
	@qemu-system-i386 -drive file=floppy.img,if=floppy,format=raw -boot a -m 128 -debugcon file:/tmp/rastapopoulos.log -nographic -s -S -no-reboot

run_graphic: floppy.img
	@qemu-system-i386 -drive file=floppy.img,if=floppy,format=raw -boot a -m 16 -debugcon file:/tmp/rastapopoulos.log -no-reboot

floppy.img: bootsect bootldr kernel initrd.img
	@echo "[INIT] floppy.img"
	@[ -f floppy.img ] || dd if=/dev/zero of=floppy.img bs=512 count=2880 > /dev/null
	@mformat -i floppy.img -t 80 -h 2 -n 18

	@echo "[CP] bootsect.bin"
	@dd if=bootsect/obj/bootsect.bin of=floppy.img conv=notrunc bs=1 count=3 &> /dev/null
	@dd if=bootsect/obj/bootsect.bin of=floppy.img conv=notrunc seek=61 skip=61 bs=1 &> /dev/null
	
	@echo "[CP] bootldr.bin"
	@mcopy -D o -i floppy.img bootldr/obj/bootldr.bin ::BOOTLDR
	
	@echo "[CP] kernel.bin"
	@mcopy -D o -i floppy.img kernel/obj/kernel.bin ::KERNEL

	@echo "[CP] initrd.img"
	@mcopy -D o -i floppy.img initrd.img ::INITRD

initrd.img:
	@make -C libc
	@make -C hello
	@make -C mkinitrd
	@make -C drivers/vga
	@mkinitrd/obj/mkinitrd initrd.img hello/obj/hello.bin drivers/vga/obj/vgadrv.bin

bootsect:
	@make -C bootsect

bootldr:
	@make -C bootldr

kernel:
	@make -C kernel

clean:
	@make -C bootsect clean
	@make -C bootldr clean
	@make -C kernel clean
	@make -C hello clean
	@make -C libc clean
	@make -C mkinitrd clean
	@make -C drivers/vga clean
	@rm -f *.o *.bin *.tmp *.img

