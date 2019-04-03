# use gcc compiler
CC=gcc
# use these flags
CFLAGS=-c -Wall

all: main

# to be continued
main: logger.o to_be_continue.c
	$(CC) -o proxy logger.o

# for testing
test: test.o logger.o
	$(CC) -o test test.o logger.o

test.o: test.c
	$(CC) $(CFLAGS) test.c

logger.o: logger.c
	$(CC) $(CFLAGS) logger.c

clean:
	rm -rf *.o proxy test




