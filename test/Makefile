# mblib test program makefile
all : mbtest ../libmb.a

clean:
	rm -f mbtest

mbtest :  mbtest.c
	gcc -Wall -g -o mbtest mbtest.c ../libmb.a