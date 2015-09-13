.PHONY: all clean objdir

all: objdir obj/bootsect.bin

obj/bootsect.bin: bootsect.asm bootsect.inc
	@echo "[ASM] $@"
	@nasm -f bin -o $@ bootsect.asm

objdir:
	@mkdir -p obj/

clean:
	@rm -vf obj/*

