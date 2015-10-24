.PHONY: all clean

all: obj/mkinitrd

obj/mkinitrd: mkinitrd.c
	@echo "[CC] $<"
	@cc -o $@ $<

clean:
	@rm -f obj/*
