# use gcc compiler
CC=gcc
# use these flags
CFLAGS=-c -Wall
# use to link library
CLINK=-lpthread

all: main.o logger.o routines.o network_handler.o http_header_handler.o cache_handler.o
	$(CC) -o proxy main.o logger.o routines.o network_handler.o http_header_handler.o cache_handler.o $(CLINK) && rm *.o

# to be continued
main.o: main.c logger.o
	$(CC) $(CFLAGS) main.c

routines.o: routines.c routines.h
	$(CC) $(CFLAGS) routines.c

network_handler.o: network_handler.c network_handler.h
	$(CC) $(CFLAGS) network_handler.c

http_header_handler.o: http_header_handler.c http_header_handler.h
	$(CC) $(CFLAGS) http_header_handler.c

cache_handler.o: cache_handler.c cache_handler.h
	$(CC) $(CFLAGS) cache_handler.c

logger.o: logger.c logger.h
	$(CC) $(CFLAGS) logger.c


clean:
	rm -rf *.o proxy test log




