# mblib test program makefile
all : mbtest mbtest_dyn

clean:
	rm -f mbtest mbtest_dyn

mbtest :  mbtest.c
	gcc -Wall -g -o mbtest mbtest.c ../libmb.a

mbtest_dyn: mbtest.c
	gcc -Wall -g -o mbtest_dyn mbtest.c ../libmb.so
