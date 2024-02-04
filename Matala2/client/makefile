all: simple_client async_client

simple_client : simple_client.o
	gcc -o simple_client simple_client.o -lssl -lcrypto

async_client : async_client.o
	gcc -o async_client async_client.o -lssl -lcrypto

async_client.o:async_client.c
	gcc -c async_client.c


simple_client.o: simple_client.c
	gcc -c simple_client.c

clean:
	rm -f *.o simple_client async_client
