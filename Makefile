# use gcc compiler
CC=gcc
# use these flags
CFLAGS=-c -Wall
# use to link library
CLINK=-lpthread

all: main.o logger.o
	$(CC) -o proxy main.o logger.o $(CLINK)

# to be continued
main.o: main.c logger.o
	$(CC) $(CFLAGS) main.c

logger.o: logger.c logger.h
	$(CC) $(CFLAGS) logger.c

clean:
	rm -rf *.o proxy test




