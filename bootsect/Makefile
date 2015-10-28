.PHONY: all clean objdir

all: objdir obj/bootsect.bin obj/bootsect_usb.bin

obj/bootsect_usb.bin: bootsect_usb.asm bootsect.inc
	@echo "[ASM] $@"
	@nasm -f bin -o $@ -Ox bootsect_usb.asm

obj/bootsect.bin: bootsect.asm bootsect.inc
	@echo "[ASM] $@"
	@nasm -f bin -o $@ -Ox bootsect.asm

objdir:
	@mkdir -p obj/

clean:
	@rm -f obj/*

