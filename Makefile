CC=cc
LDFLAGS= -lmemcache -L/usr/local/lib
CFLAGS=-c -Wall -I/usr/local/include

all: logio

logio: logio.o
	$(CC) $(LDFLAGS) logio.o -o logio

logio.o: logio.c
	$(CC) $(CFLAGS) logio.c

clean:
	rm -rf *.o logio

test:
	cat /var/log/apache/io.log | ./logio

testbig:
	cat ./test_big.txt | ./logio
	echo "get logio_2012-05-03_example.com" | nc 127.0.0.1 11211