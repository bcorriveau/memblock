# Makefile for mblib

all: libmb.so libmb.a

clean:
	rm -f mblib.o libmb.so libmb.a

libmb.so : mblib.o
	gcc -o libmb.so mblib.o -shared

libmb.a : mblib.o
	ar rcs libmb.a mblib.o

mblib.o : mblib.c mblib.h
	gcc -c -g -Wall mblib.c -fpic

