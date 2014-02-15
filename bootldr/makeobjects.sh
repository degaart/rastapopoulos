#!/bin/bash

for i in *.c
do
	echo "[CC] $i"
	i586-elf-gcc \
		-S \
		-std=gnu99 \
		-O0 \
		-masm=intel -march=i386 -mno-accumulate-outgoing-args \
		-fverbose-asm -fno-builtin -ffreestanding -fno-inline \
		-Wno-pointer-to-int-cast \
		$i
	i586-elf-gcc \
		-c \
		-o $(basename $i .c)".o" \
		-std=gnu99 \
		-O0 \
		-march=i386 -mno-accumulate-outgoing-args \
		-fno-builtin -ffreestanding -fno-inline \
		-Wno-pointer-to-int-cast \
		$i
	
done
