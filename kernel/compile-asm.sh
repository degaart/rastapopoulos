#!/bin/bash
set -eou pipefail

echo "[AS] $1"

nasm -f elf32 -o "obj/$(basename "$1").o" "$1"

