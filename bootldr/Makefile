objects := $(patsubst %.c,%.o,$(wildcard *.c))

bootldr.bin: bootldr.ld $(objects) bootldr_stub.bin
	i586-elf-gcc \
		-T bootldr.ld -o bootldr.bin -nostdlib \
		bootldr_stub.bin $(objects)

%.o: %.c
	i586-elf-gcc -c -o $@ -O0 -ffreestanding $<
	
bootldr_stub.bin: bootldr_stub.asm
	nasm -f elf32 -o bootldr_stub.bin bootldr_stub.asm

clean:
	@rm -vf *.ao *.o *.bin *.tmp

