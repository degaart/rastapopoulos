.PHONY: clean all

CFLAGS=-I../../common

all: obj obj/crc32

obj:
	mkdir -p obj

obj/crc32: obj/crc32.o obj/main.o
	cc -o obj/crc32 $(CFLAGS) obj/crc32.o obj/main.o

obj/crc32.o: ../../common/crc32.c ../../common/crc32.h
	cc -c -o obj/crc32.o $(CFLAGS) ../../common/crc32.c

obj/main.o: main.c
	cc -c -o obj/main.o $(CFLAGS) main.c

clean:
	rm -f obj/*


