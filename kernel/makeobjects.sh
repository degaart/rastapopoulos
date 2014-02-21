#!/bin/bash

for i in *.c
do
	echo "[CC]" $(basename "$i" .c)'.o'
	i586-elf-gcc \
		-c \
		-o $(basename $i .c)".o" \
		-std=gnu99 \
		-O0 \
		-march=i386 -mno-accumulate-outgoing-args \
		-fno-builtin -ffreestanding -fno-inline \
		-Wall -Wno-pointer-to-int-cast -Wno-unused-function \
		$i
	
done

