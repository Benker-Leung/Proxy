# use gcc compiler
CC=gcc
# use these flags
CFLAGS=-c -Wall
# use to link library
CLINK=-lpthread

all: main.o logger.o network_handler.o
	$(CC) -o proxy main.o logger.o network_handler.o $(CLINK)

# to be continued
main.o: main.c logger.o
	$(CC) $(CFLAGS) main.c

logger.o: logger.c logger.h
	$(CC) $(CFLAGS) logger.c

network_handler.o: network_handler.c network_handler.h
	$(CC) $(CFLAGS) network_handler.c

clean:
	rm -rf *.o proxy test log




