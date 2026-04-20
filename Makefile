CC = gcc
CFLAGS = -Wall -Werror -Wextra -pedantic -std=gnu99 -lpthread 

.PHONY: all clean

all: proj2

clean:
	rm proj2

proj2: proj2.c
	$(CC) $(CFLAGS) $^ -o $@ 