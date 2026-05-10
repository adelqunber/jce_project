#include <stdio.h>
#include <stdlib.h>

#include "graph.h"
#include "dijkstra.h"
#include "input_reader.h"
#include "GUI.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    Graph* graph = NULL;
    int src, dst;

    if (!read_graph_file(argv[1], &graph, &src, &dst)) {
        return 1;
    }

    int* path = NULL;
    int path_length = 0;
    long long total_weight = 0;

    int result = dijkstra(
        graph,
        src,
        dst,
        &path,
        &path_length,
        &total_weight
    );

    if (result == -1) {
        fprintf(stderr, "Error: Dijkstra failed\n");
        free_graph(graph);
        return 1;
    }

    if (result == 0) {
        show_graph_animation(graph, NULL, 0);
        free_graph(graph);
        return 0;
    }

    show_graph_animation(graph, path, path_length);

    free(path);
    free_graph(graph);

    return 0;
}