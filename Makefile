# use gcc compiler
CC=gcc
# use these flags
CFLAGS=-c -Wall
# use to link library
CLINK=-lpthread

all: main.o logger.o network_handler.o
	$(CC) -o proxy main.o logger.o network_handler.o $(CLINK)

unit_test: unit_test.o network_handler.o logger.o
	$(CC) -o unit_test unit_test.o network_handler.o logger.o

# to be continued
main.o: main.c logger.o
	$(CC) $(CFLAGS) main.c

unit_test.o: unit_test.c
	$(CC) $(CFLAGS) unit_test.c

network_handler.o: network_handler.c network_handler.h logger.o
	$(CC) $(CFLAGS) network_handler.c

logger.o: logger.c logger.h
	$(CC) $(CFLAGS) logger.c


clean:
	rm -rf *.o proxy test log unit_test




