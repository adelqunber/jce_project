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
make milestone7
./sim -schd fcfs input.txt
./sim -schd sjf input.txt
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
* After the second separator: travelers list for milestones 4–7

This allows the same input file to support all milestones.

Example:

```text
7 6
0 3 3
1 3 3
2 3 3
3 4 1
3 5 10
3 6 5
---
0 6
---
3
0 5 2
1 4 1
2 6 3
```

In the travelers section, each traveler line has:

```text
src dst [priority]
```

The third number is optional. If it is missing, the priority is treated as `0`.

For Milestone 7, FCFS uses the arrival order to the node queue. SJF uses the shortest remaining path distance.

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

## Milestone 7

Adds scheduling algorithms for controlling which traveler enters a busy node next.

The parent process manages a waiting queue for each node. When several travelers wait for the same node, the parent chooses the next traveler according to the selected scheduler.

Two scheduling algorithms are supported:

* `fcfs` - First Come First Served: the traveler that requested the node first enters first.
* `sjf` - Shortest Job First: the traveler with the shortest remaining path distance enters first.

Compile:

```bash
make milestone7
```

Run with FCFS:

```bash
./sim -schd fcfs input.txt
```

Run with SJF:

```bash
./sim -schd sjf input.txt
```

The GUI shows which scheduling algorithm is currently running.

Comparison:

With FCFS, travelers enter according to request order.
With SJF, travelers with shorter remaining paths enter first, so short trips may finish earlier, while longer trips may wait more.
