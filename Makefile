CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -g

RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib)

M1_SRC = main.c graph.c dijkstra.c
SIM_SRC = main.c graph.c dijkstra.c GUI.c

all: milestone3

milestone1: dijkstra

milestone2: sim

milestone3: sim

dijkstra: $(M1_SRC)
	$(CC) $(CFLAGS) -DMILESTONE1 -o dijkstra $(M1_SRC)

sim: $(SIM_SRC)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o sim $(SIM_SRC) $(RAYLIB_LIBS) -lm

clean:
	rm -f dijkstra sim