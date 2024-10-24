CC=gcc
CFLAGS=-g -Wall
OBJS_SERVER=server.o libimplserv.a

all:  $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o server $(OBJS_SERVER) -lpthread

libimplserv.a: impl_serv.o
	ar rc libimplserv.a impl_serv.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS_SERVER) impl_serv.o server