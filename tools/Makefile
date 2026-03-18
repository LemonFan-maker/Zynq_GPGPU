CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11

all: gpuasm

gpuasm: gpuasm.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f gpuasm

.PHONY: all clean
