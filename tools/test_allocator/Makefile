.PHONY: all clean

all: heap.dylib


heap.dylib: heap.o malloc.o
	cc -o heap.dylib -dynamiclib heap.o malloc.o

heap.o: heap.c list.h
	cc -o heap.o -c heap.c

malloc.o: malloc.c
	cc -o malloc.o -c malloc.c


