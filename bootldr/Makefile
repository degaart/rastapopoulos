.SUFFIXES:

objects := $(patsubst %.c,%.o,$(wildcard *.c))

bootldr.bin: bootldr.ld $(wildcard *.c) bootldr_stub.bin
	@./makeobjects.sh
	@echo "[LD] bootldr.bin"
	@i586-elf-gcc \
		-T bootldr.ld -o bootldr.bin \
		-nostdlib \
		-static \
		bootldr_stub.bin $(objects)

bootldr_stub.bin: bootldr_stub.asm
	@echo "[ASM] bootldr_stub.o"
	@nasm -f elf32 -o bootldr_stub.bin bootldr_stub.asm > /dev/null

clean:
	@rm -vf *.ao *.o *.bin *.tmp *.s *.elf

