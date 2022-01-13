CC = gcc
CFLAGS = -Wall -g

.PHONY: all clean

all: server sender receiver

server: server.o
		$(CC) -pthread -o server server.o

sender: sender.o
		$(CC) -o sender sender.o

receiver: receiver.o
		$(CC) -o receiver receiver.o

server.o: server.c
sender.o: sender.c
receiver.o: receiver.c

clean:
		rm -f *.o server
		rm -f *.o sender
		rm -f *.o receiver
