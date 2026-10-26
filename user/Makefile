.PHONY: all clean

TARGETS := lib snek shell test_vga snek2
FILES := snek/build/snek.bin \
		 shell/build/shell.bin \
		 test_vga/build/test_vga.bin \
		 snek2/build/snek2.bin
LIBS := lib/build/libcrt0.a lib/build/librasta.a

all: build/user.tar

$(LIBS):
	make -C lib

snek/build/snek.bin: $(LIBS)
	make -C snek

shell/build/shell.bin: $(LIBS)
	make -C shell

test_vga/build/test_vga.bin: $(LIBS)
	make -C test_vga

snek2/build/snek2.bin: $(LIBS)
	make -C snek2

clean:
	rm -rf build
	for target in $(TARGETS); do make -C "$$target" clean; done

build/user.tar: $(FILES) | build
	rm -rf build/tmp
	mkdir -p build/tmp
	cp -a $(FILES) build/tmp
	cp logo.raw build/tmp
	echo 'ALL YOUR BASE ARE BELONG TO US' > build/tmp/message
	cd build
	tar cvf build/user.tar -C build/tmp .

build:
	mkdir -p build

