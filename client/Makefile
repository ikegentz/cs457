CC=g++-8
CFLAGS=-Wall -Wextra -pedantic -c
LFLAGS=-I.
L_LIBRARIES=-lpthread
UTILS_PATH=../utils
OBJS=$(UTILS_PATH)/buffer.o main.o tcp_client_socket.o
EXE=client

all: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) $(L_LIBRARIES) -o $(EXE)

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp

tcp_client_socket.o: tcp_client_socket.cpp tcp_client_socket.h
	$(CC) $(CFLAGS) tcp_client_socket.cpp

clean:
	rm -rf $(OBJS) $(EXE)
