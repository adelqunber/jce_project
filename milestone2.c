#include <stdio.h>

#include "graph.h"
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

    (void)src;
    (void)dst;

    show_graph_static(graph);

    free_graph(graph);

    return 0;
}