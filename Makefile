.PHONY: boot/bootsect.bin bootldr/bootldr.bin kernel/kernel.bin

run: floppy.img
	@qemu-system-i386 -fda floppy.img -boot a
	
debug: floppy.img
	@/opt/bochs/bin/bochs -q -f bochsrc

floppy.img: boot/bootsect.bin bootldr/bootldr.bin kernel/kernel.bin
	@echo "[INIT] floppy.img"
	@[ -f floppy.img ] || dd if=/dev/zero of=floppy.img bs=512 count=2880 > /dev/null
	@mformat -i floppy.img -t 80 -h 2 -n 18
	
	@echo "[CP] bootsect.bin"
	@dd if=boot/bootsect.bin of=floppy.img conv=notrunc bs=1 count=3 &> /dev/null
	@dd if=boot/bootsect.bin of=floppy.img conv=notrunc seek=61 skip=61 bs=1 &> /dev/null
	
	@echo "[CP] bootldr.bin"
	@mcopy -D o -i floppy.img bootldr/bootldr.bin ::BOOTLDR
	
	@echo "[CP] kernel.bin"
	@mcopy -D o -i floppy.img kernel/kernel.bin ::KERNEL

boot/bootsect.bin:
	@( cd boot && make bootsect.bin; )

bootldr/bootldr.bin:
	@( cd bootldr && make bootldr.bin; )

kernel/kernel.bin:
	@( cd kernel && make kernel.bin; )

clean:
	@( cd boot && make clean; )
	@( cd bootldr && make clean; )
	@( cd kernel && make clean; )
	@rm -vf *.o *.bin *.tmp *.img

