.PHONY: clean

CC=/opt/llvm-3.7/bin/clang
LD=/Volumes/Kratos/Projects/RastapopoulOS/bin/i586-elf-ld

CFLAGS = \
	-target i586-elf-code16 \
	-std=c99 -O0 -nostdlib \
	-m16 -march=i386 -ffreestanding

ASMFLAGS = --32 -march=i386

LDFLAGS = \
    -T Test.ld \
    -Map test.map

test.bin: test.o
	$(LD) -o test.bin test.o $(LDFLAGS)

test.o: test.c
	$(CC) -o test.o -c $< $(CFLAGS)

clean:
	rm -f *.o *.map

