include $(dir $(lastword $(MAKEFILE_LIST)))/config.mk

all: obj/$(PROGRAM).a

obj/$(PROGRAM).a: $(ASM_OBJS) $(OBJS)
	@ echo "[AR] $^"
	@ $(AR) rcu $@ $^
	@ $(RANLIB) $@

obj/%.c.o: %.c
	@ echo "[CC] $@"
	@ $(CC) -o $@ -c $< $(CPPFLAGS) $(CFLAGS)

obj/%.asm.o: %.asm
	@ echo "[ASM] $@"
	@ $(AS) -o $@ -f elf32 $<

clean:
	@rm -f obj/*

obj/Makefile.depends: $(SRCS)
	@ CPP="$(CPP)" CPPFLAGS="$(CPPFLAGS)" $(BASEDIR)/makedepend.sh $^

-include obj/Makefile.depends
