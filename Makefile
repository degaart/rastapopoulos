.PHONY: all bootsect bootldr kernel floppy.img

all: floppy.img

debug: floppy.img
	@/opt/bochs/bin/bochs -q -f bochsrc -rc bochs.init

run: floppy.img
	@qemu-system-i386 -drive file=floppy.img,if=floppy,format=raw -boot a -m 16 -debugcon file:/tmp/rastapopoulos.log -nographic

floppy.img:
	@echo "[MAKE] bootsect"
	@make bootsect

	@echo "[MAKE] bootldr"
	@make bootldr

	@echo "[MAKE] kernel"
	@make kernel

	@echo "[INIT] floppy.img"
	@[ -f floppy.img ] || dd if=/dev/zero of=floppy.img bs=512 count=2880 > /dev/null
	@mformat -i floppy.img -t 80 -h 2 -n 18

	@echo "[CP] bootsect.bin"
	@dd if=boot/obj/bootsect.bin of=floppy.img conv=notrunc bs=1 count=3 &> /dev/null
	@dd if=boot/obj/bootsect.bin of=floppy.img conv=notrunc seek=61 skip=61 bs=1 &> /dev/null
	
	@echo "[CP] bootldr.bin"
	@mcopy -D o -i floppy.img bootldr/obj/bootldr.bin ::BOOTLDR
	
	@echo "[CP] kernel.bin"
	@mcopy -D o -i floppy.img kernel/obj/kernel.bin ::KERNEL

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

