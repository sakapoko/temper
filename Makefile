# Makefile for temperature reader
CC=gcc
CFLAGS=-Wall

all: temper

temper: temper.c pcsensor.c pcsensor.h
	$(CC) $(CFLAGS) temper.c pcsensor.c -o temper -lusb-1.0

clean:
	rm temper

install:
	install temper /usr/local/bin

uninstall:
	rm -f /usr/local/bin/temper
