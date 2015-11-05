.PHONY: all clean

all: obj/gensyms

obj/gensyms: gensyms.c
	@echo "[CC] $<"
	@cc -o $@ $<

clean:
	@rm -f obj/*
