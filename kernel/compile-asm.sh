#!/bin/bash
set -eou pipefail

nasm -f elf32 -o "obj/$1.o" "$1"

