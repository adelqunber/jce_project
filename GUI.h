#ifndef GUI_H
#define GUI_H

#include "graph.h"

void show_graph_static(const Graph* graph);
void show_graph_animation(const Graph* graph, const int* path, int path_length);

#endif