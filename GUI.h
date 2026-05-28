#ifndef GUI_H
#define GUI_H

#include <sys/types.h>
#include "graph.h"

void show_graph_static(const Graph* graph);
void show_graph_animation(const Graph* graph, const int* path, int path_length);

typedef struct {
    pid_t pid;
    int   pipe_fd;
    int*  path;
    int   path_length;
} TravelerState;

void show_graph_multi_animation(
    const Graph*   graph,
    TravelerState* travelers,
    int            num_travelers
);

#endif