SHELL=/bin/bash

default: all

top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

SERVER_OBJS=gpnetbenchServer.o
CLIENT_OBJS=gpnetbenchClient.o

CFLAGS=-Werror -Wall -g -O2

all: gpnetbenchServer gpnetbenchClient

gpnetbenchServer: $(SERVER_OBJS)
	$(CC) -o gpnetbenchServer $(SERVER_OBJS)

gpnetbenchClient: $(CLIENT_OBJS)
	$(CC) -o gpnetbenchClient $(CLIENT_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rf *.o gpnetbenchServer gpnetbenchClient

install: all
	mkdir -p $(prefix)/bin/lib
	cp -p gpnetbenchServer $(prefix)/bin/lib
	cp -p gpnetbenchClient $(prefix)/bin/lib
