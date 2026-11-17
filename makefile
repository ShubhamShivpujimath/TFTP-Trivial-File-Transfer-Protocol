CC = gcc
CFLAGS = -Wall -Wextra -g
SOURCES = file.c udp.c tftp.c
HEADERS = tftp.h udp.h file.h

all: client server

server: server.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) server.c $(SOURCES) -o server
client: client.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) client.c $(SOURCES) -o client

clean:
	rm -f client server