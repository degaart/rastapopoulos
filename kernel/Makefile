.PHONY: depend clean
.SUFFIXES:

SRCS = \
	bitmap.c \
	gdt.c \
	kidt.c \
	kmain.c \
	kmalloc.c \
	kstring.c \
	kterm.c \
	kutil.c \
	ll.c \
	pagedir.c \
	pic.c \
	pit.c \
	pmm.c \
	process.c \
	vmm.c

SRCS_ASM = \
	kstub.asm

CFLAGS = \
	-std=gnu99 \
	-O0 \
	-march=i386 -mno-accumulate-outgoing-args \
	-fno-builtin -ffreestanding -fno-inline \
	-Wall -Wno-pointer-to-int-cast -Wno-unused-function

LDFLAGS = \
	-nostdlib \
	-static

kernel.bin: kstub.ao $(SRCS:%.c=%.o)
	@echo "[LD] kernel.bin"
	@i586-elf-ld \
		-T kernel.ld -o $@ \
		-Map kernel.map \
		$(LDFLAGS) \
		$^

%.o: %.c
	@echo "[CC] $<"
	@i586-elf-gcc \
		-c \
		$(CFLAGS) \
		-o $@ \
		$<


kstub.ao: kstub.asm isr_stub.inc
	@echo "[ASM] kstub.ao"
	@nasm -f elf32 -o kstub.ao kstub.asm > /dev/null

clean:
	@rm -vf *.ao *.o *.bin *.tmp *.s *.elf *.map Makefile.depends

hello.h: hello.bin
	@echo "[BIN] hello.h"
	@bin2c -o hello.h hello.bin

hello.bin: hello.asm
	@echo "[ASM] hello.asm"
	@nasm -f bin -o hello.bin hello.asm

depend: $(SRCS)
	@rm -f Makefile.depends
	@for i in $(SRCS); do  \
		echo "[DEP] $$i"; \
		i586-elf-cpp $(CFLAGS) -MM $$i >> Makefile.depends ; \
	done

# Dependency include file
# The hyphen avoids erros when the dependency file does not exists
-include Makefile.depends

