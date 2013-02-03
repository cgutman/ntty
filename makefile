# Ntty Makefile

CC=gcc
CFLAGS=-Wall -Werror

all: ntty

ntty: ntty.c
	$(CC) $(CFLAGS) ntty.c -o ntty -pthread

clean:
	rm -f ntty
