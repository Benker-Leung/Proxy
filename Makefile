# use gcc compiler
CC=gcc
# use these flags
CFLAGS=-c -Wall
# use to link library
CLINK=-lpthread

all: main.o logger.o network_handler.o http_header_handler.o
	$(CC) -o proxy main.o logger.o network_handler.o http_header_handler.o $(CLINK)

# to be continued
main.o: main.c logger.o
	$(CC) $(CFLAGS) main.c

network_handler.o: network_handler.c network_handler.h logger.o
	$(CC) $(CFLAGS) network_handler.c

http_header_handler.o: http_header_handler.c http_header_handler.h
	$(CC) $(CFLAGS) http_header_handler.c

logger.o: logger.c logger.h
	$(CC) $(CFLAGS) logger.c


clean:
	rm -rf *.o proxy test log unit_test




