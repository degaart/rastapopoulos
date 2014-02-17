.SUFFIXES:

kernel.bin: kstub.ao kernel.ld $(wildcard *.c) $(wildcard *.h)
	@./makeobjects.sh
	@echo "[LD] kernel.bin"
	@i586-elf-ld\
		-T kernel.ld -o kernel.bin \
		-nostdlib \
		-static \
		-Map kernel.map \
		kstub.ao *.o

kstub.ao: kstub.asm isr_stub.inc
	@echo "[ASM] kstub.ao"
	@nasm -f elf32 -o kstub.ao kstub.asm > /dev/null

clean:
	@rm -vf *.ao *.o *.bin *.tmp *.s *.elf

