# Graph Movement Simulation

Operating Systems project in C on Linux.

The program reads a directed weighted graph from a text file, runs Dijkstra, displays the graph with raylib, and animates movement on the shortest path.

## Files

- `milestone1.c` - Dijkstra terminal program
- `milestone2.c` - Static GUI graph display
- `milestone3.c` - Animated movement on shortest path
- `graph.c/h` - Graph structure
- `dijkstra.c/h` - Dijkstra algorithm
- `input_reader.c/h` - Input file reader
- `GUI.c/h` - raylib GUI
- `input.txt` - Example input
- `Makefile` - Build commands

## Milestone 1

Compile: `make milestone1`  
Run: `./dijkstra input.txt`

## Milestone 2

Compile: `make milestone2`  
Run: `./sim input.txt`

## Milestone 3

Compile: `make milestone3`  
Run: `./sim input.txt`

## Clean

Run: `make clean`

## Input Format

N M  
src dst weight  
src dst weight  
...  
query_src query_dst

## Example

6 8  
0 1 4  
0 2 2  
1 3 5  
2 1 1  
2 3 8  
3 4 2  
4 5 3  
2 5 10  
0 5

Expected Milestone 1 output:

0 -> 2 -> 5  
12
