.SUFFIXES:
.PHONY: all clean

all: obj/hello.exe

obj/hello.exe: hello.asm 
	@echo "[ASM] $^"
	@nasm -o $@ -f bin $<

clean:
	@rm -f obj/*

