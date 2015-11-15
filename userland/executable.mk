include $(dir $(lastword $(MAKEFILE_LIST)))/config.mk

all: obj/$(PROGRAM).bin

obj/$(PROGRAM).bin: obj/$(PROGRAM).elf
	@echo "[CONV] $^"
	@$(OBJCOPY) -O binary $^ $@

obj/$(PROGRAM).elf: $(ASM_OBJS) $(OBJS)
	@echo "[LD] $^"
	@$(LD) \
		-T $(BASEDIR)/executable.ld -o $@ \
		-Map obj/$(PROGRAM).map \
		$^ \
		$(LDFLAGS) \
		-lc

obj/%.c.o: %.c
	@ echo "[CC] $@"
	@ $(CC) -o $@ -c $< $(CPPFLAGS) $(CFLAGS)

obj/%.asm.o: %.asm
	@ echo "[ASM] $@"
	@ $(AS) -o $@ -f elf32 $<

obj/Makefile.depends: $(SRCS)
	@ CPP="$(CPP)" CPPFLAGS="$(CPPFLAGS)" $(BASEDIR)/makedepend.sh $^

clean:
	@rm -f obj/*

-include obj/Makefile.depends
