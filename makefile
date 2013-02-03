# Ntty Makefile

CC=gcc
CFLAGS=-c -Wall -Werror

all: ntty

ntty: ntty.c
	$(CC) $(CFLAGS) ntty.c -o ntty

clean:
	rm -f ntty
