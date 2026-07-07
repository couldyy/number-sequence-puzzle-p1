CC=gcc

all: solver

solver: solver.c thirdparty/marena.h
	$(CC) -O3 -o solver solver.c

