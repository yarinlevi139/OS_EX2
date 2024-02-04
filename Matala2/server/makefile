all: server

server: server.o
	gcc -o server server.o -lssl -lcrypto

server.o : server.c
	gcc -c server.c

clean:
	rm -f *.o server