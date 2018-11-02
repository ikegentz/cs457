CC=g++-8
CFLAGS=-Wall -Wextra -pedantic -c
LFLAGS=-I.
L_LIBRARIES=-lpthread
OBJS=buffer.o main.o tcp_server_socket.o tcp_user_socket.o
EXE=server

all: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) $(L_LIBRARIES) -o $(EXE)

buffer.o: buffer.cpp
	$(CC) $(CFLAGS) buffer.cpp

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp

tcp_server_socket.o: tcp_server_socket.cpp tcp_server_socket.h
	$(CC) $(CFLAGS) tcp_server_socket.cpp

tcp_user_socket.o: tcp_user_socket.cpp tcp_user_socket.h
	$(CC) $(CFLAGS) tcp_user_socket.cpp

clean:
	rm -rf $(OBJS) $(EXE) $(OUTPUT_DIRS)
