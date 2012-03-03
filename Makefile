all: dro2imf

dro2imf: dro2imf.c
	$(CC) -pedantic --std=c99 -Wall -o $@ $<
