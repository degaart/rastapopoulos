.PHONY: all clean run
.SUFFIXES:
.SECONDARY:

all:
	make -C user
	make -C kernel
	make -C bootloader

clean:
	make -C user clean
	make -C kernel clean
	make -C bootloader clean

run:
	make -C bootloader run

