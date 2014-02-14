#!/bin/bash

for i in *.c
do
	echo "[CC] $i"
	i586-elf-gcc -c -o $(basename $i .c)".o" -O0 -ffreestanding $i
done
