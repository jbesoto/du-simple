CC=gcc
FLAGS=-O2 -Wall -Wextra

all: du

du: du.c
	${CC} ${FLAGS} du.c -o du

clean:
	rm -rf du

.PHONY: clean