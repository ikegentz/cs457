CC=g++-8
CFLAGS=-Wall -Wextra -pedantic -c
LFLAGS=-I.
OBJS=main.o driver.o model.o wavefront_obj.o
OUTPUT_DIRS=driver0*/
EXE=modeltoworld

all: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o $(EXE)

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp

driver.o: driver.cpp driver.h
	$(CC) $(CFLAGS) driver.cpp

model.o: model.cpp model.h
	$(CC) $(CFLAGS) model.cpp

wavefront_obj.o: wavefront_obj.cpp wavefront_obj.h
	$(CC) $(CFLAGS) wavefront_obj.cpp

clean:
	rm -rf $(OBJS) $(EXE) $(OUTPUT_DIRS)
