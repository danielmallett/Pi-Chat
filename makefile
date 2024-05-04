CC = gcc 
INCLUDES = -Iinclude
CFLAGS = $(INCLUDES) -O3 -lpthread
LIBDIR = lib
LIBS = $(LIBDIR)/libfdr.a

EXECUTABLES = bin/chat-server
LIBRARIES = lib/libfdr.a obj/sockettome.o
OBJS = obj/dllist.o obj/fields.o obj/jval.o obj/jrb.o

all: $(EXECUTABLES) $(LIBRARIES) src/chat-server.c

bin/chat-server: $(OBJS) $(LIBRARIES) src/chat-server.c obj/chat-server.o
	$(CC) $(CFLAGS) -o bin/chat-server obj/chat-server.o obj/sockettome.o $(OBJS)

lib/libfdr.a: $(OBJS)
	ar ru lib/libfdr.a $(OBJS)
	ranlib lib/libfdr.a

obj/chat-server.o: $(OBJS) $(LIBRARIES) src/chat-server.c
	$(CC) $(CFLAGS) -c -o obj/chat-server.o src/chat-server.c

obj/fields.o: src/fields.c include/fields.h
	$(CC) $(CFLAGS) -c -o obj/fields.o src/fields.c

obj/jval.o: src/jval.c include/jval.h
	$(CC) $(CFLAGS) -c -o obj/jval.o src/jval.c

obj/dllist.o: src/dllist.c include/dllist.h include/jval.h
	$(CC) $(CFLAGS) -c -o obj/dllist.o src/dllist.c

obj/jrb.o: src/jrb.c include/jrb.h include/jval.h
	$(CC) $(CFLAGS) -c -o obj/jrb.o src/jrb.c

obj/sockettome.o: src/sockettome.c include/sockettome.h
	$(CC) $(CFLAGS) -c -o obj/sockettome.o src/sockettome.c

clean:
	rm -f obj/* bin/* lib/*


