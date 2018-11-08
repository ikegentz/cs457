CC=g++-8
CFLAGS=-Wall -Wextra -pedantic -c
LFLAGS=-I.
L_LIBRARIES=-lpthread
UTILS_PATH=../utils
OBJS=$(UTILS_PATH)/buffer.o main.o tcp_server_socket.o tcp_user_socket.o user.o
EXE=server

all: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) $(L_LIBRARIES) -o $(EXE)

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp

tcp_server_socket.o: tcp_server_socket.cpp tcp_server_socket.h
	$(CC) $(CFLAGS) tcp_server_socket.cpp

tcp_user_socket.o: tcp_user_socket.cpp tcp_user_socket.h
	$(CC) $(CFLAGS) tcp_user_socket.cpp

user.o: user.cpp user.h
	$(CC) $(CFLAGS) user.cpp

clean:
	rm -rf $(OBJS) $(EXE) $(OUTPUT_DIRS)
