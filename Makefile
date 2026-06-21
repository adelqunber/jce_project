CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -g

RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib)

.PHONY: all milestone1 milestone2 milestone3 milestone4 milestone5 milestone6 milestone7 clean

all: milestone7

milestone1:
	$(CC) $(CFLAGS) -o dijkstra milestone1.c input_reader.c graph.c dijkstra.c

milestone2:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o sim milestone2.c input_reader.c graph.c GUI.c scheduler.c $(RAYLIB_LIBS) -lm

milestone3:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o sim milestone3.c input_reader.c graph.c dijkstra.c GUI.c scheduler.c $(RAYLIB_LIBS) -lm

milestone4:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o sim milestone4.c input_reader.c graph.c dijkstra.c GUI.c scheduler.c $(RAYLIB_LIBS) -lm

milestone5:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o sim milestone5.c input_reader.c graph.c dijkstra.c GUI.c scheduler.c $(RAYLIB_LIBS) -lm

milestone6:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o sim milestone6.c input_reader.c graph.c dijkstra.c GUI.c scheduler.c $(RAYLIB_LIBS) -lm -pthread

milestone7:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o sim milestone7.c input_reader.c graph.c dijkstra.c GUI.c scheduler.c $(RAYLIB_LIBS) -lm -pthread

clean:
	rm -f dijkstra sim