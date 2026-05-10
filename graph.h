#ifndef GRAPH_H
#define GRAPH_H

typedef struct Edge {
    int dst;
    int weight;
    struct Edge* next;
} Edge;

typedef struct Graph {
    int num_nodes;
    int num_edges;
    Edge** adj;
} Graph;

Graph* create_graph(int num_nodes);
int add_edge(Graph* graph, int src, int dst, int weight);
void free_graph(Graph* graph);

#endif