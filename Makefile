CC=cc
LDFLAGS= -lmemcache -L/usr/local/lib
CFLAGS=-c -g -Wall -I/usr/local/include

all: logio

logio: logio.o
	$(CC) $(LDFLAGS) logio.o -o logio

logio.o: logio.c
	$(CC) $(CFLAGS) logio.c

clean:
	rm -rf *.o logio

test:
	cat /var/log/apache/io.log | ./logio