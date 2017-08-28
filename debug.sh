#!/bin/bash

symbols="${1-kernel/obj/kernel.elf}"



exec i386-pc-elf-gdb -tui --command=debug.gdb --symbols="$symbols"


