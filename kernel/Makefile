kernel.bin: kernel.asm string.inc
	@echo "[ASM] kernel.asm"
	@nasm -f bin -o kernel.bin kernel.asm > /dev/null

clean:
	@rm -vf *.o *.bin *.tmp
