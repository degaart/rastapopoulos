kernel.bin: kmain.o kheader.o kernel.ld
	@echo "[LD] kernel.bin"
	@i586-elf-gcc \
		-T kernel.ld -o kernel.bin \
		-nostdlib \
		-static \
		kheader.o kmain.o

kmain.o: kmain.c
	@echo "[CC] kernel.o"
	@i586-elf-gcc \
		-c \
		-o kmain.o \
		-std=gnu99 \
		-O0 \
		-march=i386 -mno-accumulate-outgoing-args \
		-fno-builtin -ffreestanding -fno-inline \
		-Wno-pointer-to-int-cast \
		kmain.c

kheader.o: kheader.asm
	@echo "[ASM] kheader.o"
	@nasm -f elf32 -o kheader.o kheader.asm > /dev/null

clean:
	@rm -vf *.ao *.o *.bin *.tmp *.s *.elf

