.PHONY: all clean run
.SUFFIXES:
.SECONDARY:

all:
	make -C kernel
	make -C bootloader

clean:
	make -C kernel clean
	make -C bootloader clean

run:
	make -C bootloader run

