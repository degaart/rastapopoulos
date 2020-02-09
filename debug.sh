#!/bin/bash
symbols="${1-kernel/obj/kernel.elf}"
exec i686-elf-gdb -tui --command=debug.gdb --symbols="$symbols"

