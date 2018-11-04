CC=g++-8
CFLAGS=-Wall -Wextra -pedantic -c
OBJS=buffer.o
EXE=server

buffer.o: buffer.cpp
	$(CC) $(CFLAGS) buffer.cpp

clean:
	rm -rf $(OBJS) $(EXE) $(OUTPUT_DIRS)
