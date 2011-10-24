CC=gcc
CFLAGS=-ggdb

all: library

library:
	$(CC) $(CFLAGS) -c -fpic mem.c -Werror
	$(CC) -shared -o libmem.so mem.o

clean:
	rm -rf libmem.so
	rm -rf mem.o
