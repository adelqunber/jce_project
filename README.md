# Graph Movement Simulation

Operating Systems project in C using a directed weighted graph.

## Requirements

* Linux
* gcc
* make
* raylib

## Compile and Run

```bash
make milestone1
./dijkstra input.txt
```

```bash
make milestone2
./sim input.txt
```

```bash
make milestone3
./sim input.txt
```

```bash
make milestone4
./sim input.txt
```

```bash
make milestone5
./sim input.txt
```

```bash
make milestone6
./sim input.txt
```

```bash
make clean
```

## Input Format

The input file is divided using separator lines:

```text
---
```

The separators split the file into sections:

```text
graph section
---
single source/destination query
---
multiple travelers section
```

Meaning:

* Before the first separator: graph definition
* After the first separator: single query for milestones 1–3
* After the second separator: travelers list for milestones 4–6

This allows the same input file to support all milestones.

Example:

```text
7 6
0 3 5
1 3 5
2 3 5
3 4 12
4 5 12
5 6 12
---
0 6
---
3
0 6
1 6
2 6
```

## Milestone 1

Reads a directed weighted graph from a file and runs Dijkstra to print the shortest path and total weight.

## Milestone 2

Displays the graph in a raylib GUI with nodes, directed edges, labels, and edge weights.

## Milestone 3

Animates one traveler on the shortest path calculated by Dijkstra.

The traveler moves according to edge weights and waits 1 second at intermediate nodes.

## Milestone 4

Adds multiple travelers using child processes created with `fork`.

The parent calculates all paths and controls the GUI animation.
Each traveler is shown with a different color.

## Milestone 5

Each child process calculates its own Dijkstra path and sends updates to the parent.

IPC mechanism used: `POSIX pipes`.

A separate pipe is created for each child.
The child sends `PathMsg` and `TravelerMsg` messages to the parent.

Pipes were chosen because communication is one-way from child to parent.
The children only send path and movement updates, so pipes are simpler than shared memory for this milestone.

The parent reads the messages, updates the GUI, and prints movement logs.

## Milestone 6

Adds synchronized access to nodes.

IPC is still done with `POSIX pipes`.

Synchronization mechanism used: `POSIX semaphores`.

Implementation:

* one semaphore per node
* semaphores are stored in shared memory using `mmap`
* each node semaphore starts with value `1`
* a child must lock the node before entering it
* only one traveler can be inside a node at a time
* if a node is busy, other travelers wait outside
* each traveler stays inside the node for 1 second
* the GUI shows waiting travelers with a yellow waiting sign/icon

The child sends `SyncTravelerMsg` messages to the parent so the GUI can show moving, waiting, inside-node, and finished states.
