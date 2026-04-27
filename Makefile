CC = gcc
CFLAGS = -Wall -Werror -Wextra -pedantic -std=gnu99 -lpthread 

.PHONY: all clean

all: proj2

clean:
	rm -f proj2 proj2.out 

proj2: proj2.c proj2.h
	$(CC) $(CFLAGS) $< -o $@ 