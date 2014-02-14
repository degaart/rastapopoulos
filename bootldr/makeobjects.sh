#!/bin/bash

for i in *.c
do
	echo "[CC] $i"
	i586-elf-gcc -c -o $(basename $i .c)".o" -std=gnu99 -Og -Wno-pointer-to-int-cast -ffreestanding -fno-inline $i
done
