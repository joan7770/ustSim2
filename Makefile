CC=gcc
CFLAGS=-std=c99

all: simulator
	$(CC) $(CFLAGS) $(INC) simulator.o -o simulator

simulator:
	$(CC) $(CFLAGS) $(INC) -c main.c -o simulator.o

clean:
	rm *.o *.out simulator
