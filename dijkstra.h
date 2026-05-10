#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "graph.h"

int dijkstra(
    const Graph* graph,
    int src,
    int dst,
    int** out_path,
    int* out_path_length,
    long long* out_total_weight
);

void print_path(const int* path, int path_length);

#endif