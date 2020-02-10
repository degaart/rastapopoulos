#!/bin/bash
exec i686-elf-addr2line -e kernel/obj/kernel.elf "$@"

