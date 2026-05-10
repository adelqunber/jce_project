CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -g

RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib)

.PHONY: all milestone1 milestone2 milestone3 clean

all: milestone3

milestone1:
	$(CC) $(CFLAGS) -o dijkstra milestone1.c input_reader.c graph.c dijkstra.c

milestone2:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o sim milestone2.c input_reader.c graph.c GUI.c $(RAYLIB_LIBS) -lm

milestone3:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o sim milestone3.c input_reader.c graph.c dijkstra.c GUI.c $(RAYLIB_LIBS) -lm

clean:
	rm -f dijkstra sim