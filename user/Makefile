.PHONY: all clean

TARGETS := lib snek shell fib
FILES := snek/build/snek.bin \
		 shell/build/shell.bin \
		 fib/target/i386-elf/debug/fib.bin

all: build/user.tar

snek/build/snek.bin:
	make -C snek

shell/build/shell.bin:
	make -C shell

fib/target/i386-elf/debug/fib.bin:
	make -C fib

clean:
	rm -rf build

build/user.tar: $(FILES) | build
	rm -rf build/tmp
	mkdir -p build/tmp
	cp -a $(FILES) build/tmp
	echo 'ALL YOUR BASE ARE BELONG TO US' > build/tmp/message
	cd build
	tar cvf build/user.tar -C build/tmp .

build:
	mkdir -p build


