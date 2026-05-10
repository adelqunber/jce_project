#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

Graph* create_graph(int num_nodes) {
    if (num_nodes <= 0) {
        return NULL;
    }

    Graph* graph = malloc(sizeof(Graph));
    if (graph == NULL) {
        return NULL;
    }

    graph->num_nodes = num_nodes;
    graph->num_edges = 0;

    graph->adj = calloc(num_nodes, sizeof(Edge*));
    if (graph->adj == NULL) {
        free(graph);
        return NULL;
    }

    return graph;
}

int add_edge(Graph* graph, int src, int dst, int weight) {
    if (graph == NULL) {
        return 0;
    }

    if (src < 0 || dst < 0 || weight < 0) {
        return 0;
    }

    if (src >= graph->num_nodes || dst >= graph->num_nodes) {
        return 0;
    }

    Edge* new_edge = malloc(sizeof(Edge));
    if (new_edge == NULL) {
        return 0;
    }

    new_edge->dst = dst;
    new_edge->weight = weight;
    new_edge->next = graph->adj[src];

    graph->adj[src] = new_edge;
    graph->num_edges++;

    return 1;
}

void free_graph(Graph* graph) {
    if (graph == NULL) {
        return;
    }

    for (int i = 0; i < graph->num_nodes; i++) {
        Edge* current = graph->adj[i];

        while (current != NULL) {
            Edge* temp = current;
            current = current->next;
            free(temp);
        }
    }

    free(graph->adj);
    free(graph);
}