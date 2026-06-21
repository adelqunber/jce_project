#ifndef GUI_H
#define GUI_H

#include <sys/types.h>
#include "graph.h"
#include "scheduler.h"

void show_graph_static(const Graph* graph);
void show_graph_animation(const Graph* graph, const int* path, int path_length);

typedef struct {
    pid_t pid;
    int   pipe_fd;
    int   control_fd; /* milestone 7: parent->child grant pipe; -1 if unused */
    int*  path;
    int   path_length;
} TravelerState;

void show_graph_multi_animation(
    const Graph*   graph,
    TravelerState* travelers,
    int            num_travelers
);

void show_graph_synchronized_animation(
    const Graph*   graph,
    TravelerState* travelers,
    int            num_travelers
);

/* Milestone 7: node access is granted by the parent process, which keeps a
 * waiting queue per node and wakes up the next traveler according to the
 * selected scheduling algorithm (see scheduler.h). */
void show_graph_scheduled_animation(
    const Graph*   graph,
    TravelerState* travelers,
    int            num_travelers,
    SchedAlgo      algo
);

#endif
