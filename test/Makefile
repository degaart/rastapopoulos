test_floppy.img: test.bin
	@echo "[INIT] floppy.img"
	@[ -f test_floppy.img ] || dd if=/dev/zero of=test_floppy.img bs=512 count=2880 > /dev/null
	@dd if=test.bin of=test_floppy.img bs=512 count=1

test.bin: test.asm
	@echo "[ASM] test.bin"
	@nasm -f bin -o test.bin test.asm

clean:
	@rm -f *.bin *.o

debug: test_floppy.img
	@/opt/bochs/bin/bochs -f bochsrc


	