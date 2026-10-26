.PHONY: all clean

TARGETS := lib snek shell test_vga snek2

all:
	rm -rf build/tmp
	mkdir -p build/tmp
	for target in $(TARGETS); do \
		make -C "$$target"; \
		cp "$$target/build/$${target}.bin" build/tmp; \
	done
	cp logo.raw build/tmp
	echo 'ALL YOUR BASE ARE BELONG TO US' > build/tmp/message
	cd build
	tar cvf build/user.tar -C build/tmp .

clean:
	rm -rf build
	for target in $(TARGETS); do make -C "$$target" clean; done

