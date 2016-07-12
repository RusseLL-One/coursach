all: client_rcvr.o client_sndr.o server.o
	gcc -Wall -std=c99 -o client_rcvr client_rcvr.o -lpthread -lprotobuf-c
	gcc -Wall -std=c99 -o client_sndr client_sndr.o -lpthread -lprotobuf-c
	gcc -Wall -std=c99 -o server server.o -lpthread -lprotobuf-c
client_rcvr.o: client_rcvr.c
	gcc -Wall -std=c99 -c client_rcvr.c
client_sndr.o: client_sndr.c
	gcc -Wall -std=c99 -c client_sndr.c
server.o: server.c
	gcc -Wall -std=c99 -c server.c
clean:
	rm client_rcvr.o client_sndr.o server.o client_rcvr client_sndr server
