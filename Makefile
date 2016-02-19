CC = gcc
CFLAGS = -std=gnu99 -Wall -I. -O1
CC_CMD = $(CC) $(CFLAGS) -o $@ -c $<

UNAME := $(shell uname)
LDFLAGS = -lpthread
ifeq ($(UNAME), SunOS)
LDFLAGS += -lsocket -lnsl
endif

%.o: client/%.c
	$(CC_CMD)

%.o: server/%.c
	$(CC_CMD)

%.o: common_utils/%.c
	$(CC_CMD)

default: client_main server_main

debug: CFLAGS += -D_DEBUG -g
debug: default

client_main: sysadmin.o protocol_utils.o readwrite.o client_main.o repl.o command_handlers.o

server_main: sysadmin.o protocol_utils.o readwrite.o accepter.o request_handlers.o

.PHONY: clean

clean:
	rm *.o client_main server_main
