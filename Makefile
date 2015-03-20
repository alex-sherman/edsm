CC=gcc
CFLAGS=-Wall

all: client server

client: client.c
	$(CC) $(CFLAGS) client.c -lzmq -o client

server: server.c
	$(CC) $(CFLAGS) server.c -lzmq -o server

clean:
	rm client server
