CC=g++-8
CFLAGS=-Wall -Wextra -pedantic -c
LFLAGS=-I.
OBJS=main.o
EXE=client

all: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o $(EXE)

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp

clean:
	rm -rf $(OBJS) $(EXE)
