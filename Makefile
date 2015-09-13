.PHONY: all bootsect bootldr kernel floppy.img

all: floppy.img

debug: floppy.img
	@/usr/local/bin/bochs -q -f bochsrc -rc bochs.init

run: floppy.img
	@qemu-system-i386 -drive file=floppy.img,if=floppy,format=raw -boot a -m 16 -debugcon file:/tmp/rastapopoulos.log

floppy.img:
	@echo "[MAKE] bootsect"
	@make bootsect

	@echo "[MAKE] bootldr"
	@make bootldr

	@echo "[MAKE] kernel"
	@make kernel

	@echo "[INIT] floppy.img"
	@[ -f floppy.img ] || dd if=/dev/zero of=floppy.img bs=512 count=2880 > /dev/null
	@mformat -i floppy.img -t 80 -h 2 -n 18 -B boot/obj/bootsect.bin
	
	@echo "[CP] bootldr.bin"
	@mcopy -D o -i floppy.img bootldr/bootldr.bin ::BOOTLDR
	
	@echo "[CP] kernel.bin"
	@mcopy -D o -i floppy.img kernel/kernel.bin ::KERNEL

bootsect:
	@( cd boot && make ; )

bootldr:
	@( cd bootldr && make ; )

kernel:
	@( cd kernel && make ; )

clean:
	@( cd boot && make clean; )
	@( cd bootldr && make clean; )
	@( cd kernel && make clean; )
	@rm -vf *.o *.bin *.tmp *.img

