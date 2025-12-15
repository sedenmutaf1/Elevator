CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c11 -pthread
GTKFLAGS=`pkg-config --cflags --libs gtk+-3.0`

all: elevator

simulation.o: simulation.c elevator.h
	$(CC) $(CFLAGS) -c simulation.c

# Console simulation

elevator: elevator.c simulation.o
	$(CC) $(CFLAGS) -o $@ elevator.c simulation.o

# GUI version

elevator_gui: elevator_gui.c simulation.o
	$(CC) $(CFLAGS) -o $@ elevator_gui.c simulation.o $(GTKFLAGS)

clean:
	rm -f elevator elevator_gui *.o

.PHONY: all clean
