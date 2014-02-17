.SUFFIXES:

kernel.bin: kstub.ao kernel.ld $(wildcard *.c) $(wildcard *.h)
	@./makeobjects.sh
	@echo "[LD] kernel.bin"

	@# When linking using ld, static and global data are not
	@# initialized when their value are 0
	@# So we need to use gcc
	@# gcc does not recognize the -Map switch, so we need to 
	@# pass it to ld using the -Xlinker switch
	@i586-elf-gcc \
		-T kernel.ld -o kernel.bin \
		-nostdlib \
		-static \
		-Xlinker -Map -Xlinker kernel.map \
		kstub.ao *.o

kstub.ao: kstub.asm isr_stub.inc
	@echo "[ASM] kstub.ao"
	@nasm -f elf32 -o kstub.ao kstub.asm > /dev/null

clean:
	@rm -vf *.ao *.o *.bin *.tmp *.s *.elf *.map


