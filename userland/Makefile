.SUFFIXES:
.PHONY: all clean

PROJECTS := runtime init fib logger primes sleeper

all:
	@ for proj in $(PROJECTS); do make -C $$proj; done

clean:
	@ for proj in $(PROJECTS); do make -C $$proj clean; done



