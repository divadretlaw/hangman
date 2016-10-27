CLIENT = hangman-client
SERVER = hangman-server
SHARED = hangman

platform=$(shell uname)

ifeq ($(platform),Darwin)
CC = clang
LFLAGS = -lpthread
else
CC = gcc
LFLAGS = -lrt -pthread
endif

CFLAGS = -std=c99 -pedantic -Wall -D_XOPEN_SOURCE=500 -D_BSD_SOURCE -g -c

.PHONY: all clean

all: $(CLIENT) $(CLIENT).c $(SERVER) $(SERVER).c

$(CLIENT): $(CLIENT).o
	$(CC) -o $(CLIENT) $(CLIENT).o $(LFLAGS)

$(CLIENT).o: $(CLIENT).c $(SHARED).h
	$(CC) $(CFLAGS) $(CLIENT).c

$(SERVER): $(SERVER).o
	$(CC) -o $(SERVER) $(SERVER).o $(LFLAGS)

$(SERVER).o: $(SERVER).c $(SHARED).h
	$(CC) $(CFLAGS) $(SERVER).c

clean:
	rm -f $(CLIENT) $(SERVER) *.o
