PROGRAM := vgatest
CFLAGS := -iquote ../../common -g
LDFLAGS := -g
LIBS += obj/libx86emu.a

include ../tools.mk

X86EMU_SRCS := $(wildcard libx86emu-1.12/*.c)
X86EMU_OBJS := $(patsubst libx86emu-1.12/%.c,obj/x86emu-%.o,$(X86EMU_SRCS))

obj/$(PROGRAM): obj/libx86emu.a obj/program.bin

obj/libx86emu.a: $(X86EMU_OBJS)
	@ echo "[AR] $@"
	@ $(AR) rcu $@ $^
	@ $(RANLIB) $@

obj/x86emu-%.o:
	@ echo "[CC] $@"
	@ $(CC) -c -o $@ -g -Wno-absolute-value libx86emu-1.12/$*.c

obj/program.bin: program.asm
	@ echo "[ASM] $@"
	@ $(NASM) -o $@ -f bin $<



