CC=gcc
CFLAGS=-g -pedantic -std=gnu17 -Wall -Werror -Wextra
LDFLAGS=-lpthread

.PHONY: all
all: encoder

encoder: encoder.o utils.o 
	$(CC) $(CFLAGS) encoder.o utils.o -o encoder $(LDFLAGS)

encoder.o: encoder.c utils.h
	$(CC) $(CFLAGS) -c encoder.c -o encoder.o

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c utils.c -o utils.o

.PHONY: clean
clean:
	rm -f *.o encoder


