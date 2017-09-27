.SUFFIXES:
.PHONY: all clean

PROJECTS := runtime init fib logger primes sleeper

all:
	@ make -C runtime
	@ make -C init
	@ make -C logger
	@ make -C vfs
	@ make -C blk

clean:
	@ make -C runtime clean
	@ make -C init clean
	@ make -C logger clean
	@ make -C vfs clean
	@ make -C blk clean



