.SUFFIXES:
.PHONY: all clean

PROJECTS := runtime init fib logger primes sleeper

all:
	@ make -C runtime
	@ make -C init
	@ make -C vfs
	@ make -C blk
	@ make -C vga
	@ make -C svga

clean:
	@ make -C runtime clean
	@ make -C init clean
	@ make -C vfs clean
	@ make -C blk clean
	@ make -C vga clean
	@ make -C svga clean



