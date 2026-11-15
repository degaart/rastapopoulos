.PHONY: all clean

TARGETS := lib snek shell test_vga snek2

all:
	rm -rf build/tmp
	mkdir -p build/tmp
	for target in $(TARGETS); do \
		make -C "$$target"; \
		[ -f "$$target/build/$${target}.bin" ] && cp "$$target/build/$${target}.bin" build/tmp; \
	done
	cp logo.raw build/tmp
	echo 'ALL YOUR BASE ARE BELONG TO US' > build/tmp/message
	mkdir -p build/tmp/arf/gouda
	echo 'SEICHE' > build/tmp/arf/gouda/seiche
	mkdir -p build/tmp/a/b/c/d/e/f/g/h/i/j
	echo 'k' > build/tmp/a/b/c/d/e/f/g/h/i/j/k
	cd build
	tar cvf build/user.tar -C build/tmp .

clean:
	rm -rf build
	for target in $(TARGETS); do make -C "$$target" clean; done

