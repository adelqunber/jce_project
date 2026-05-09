#include <stdio.h>
#include <stdlib.h>

#include "graph.h"
#include "dijkstra.h"

#ifndef MILESTONE1
#include "GUI.h"
#endif

static int read_graph_file(
    const char* filename,
    Graph** out_graph,
    int* out_src,
    int* out_dst
) {
    FILE* file = fopen(filename, "r");

    if (file == NULL) {
        fprintf(stderr, "Error: could not open file\n");
        return 0;
    }

    int n, m;

    if (fscanf(file, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error: invalid file format\n");
        fclose(file);
        return 0;
    }

    if (n < 0 || m < 0) {
        fprintf(stderr, "Invalid input: negative numbers are not allowed\n");
        fclose(file);
        return 0;
    }

    if (n == 0) {
        fprintf(stderr, "Error: number of nodes must be positive\n");
        fclose(file);
        return 0;
    }

    Graph* graph = create_graph(n);

    if (graph == NULL) {
        fprintf(stderr, "Error: memory allocation failed\n");
        fclose(file);
        return 0;
    }

    for (int i = 0; i < m; i++) {
        int src, dst, weight;

        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            fprintf(stderr, "Error: invalid edge format\n");
            free_graph(graph);
            fclose(file);
            return 0;
        }

        if (src < 0 || dst < 0 || weight < 0) {
            fprintf(stderr, "Invalid input: negative numbers are not allowed\n");
            free_graph(graph);
            fclose(file);
            return 0;
        }

        if (src >= n || dst >= n) {
            fprintf(stderr, "Error: node index out of range\n");
            free_graph(graph);
            fclose(file);
            return 0;
        }

        if (!add_edge(graph, src, dst, weight)) {
            fprintf(stderr, "Error: failed to add edge\n");
            free_graph(graph);
            fclose(file);
            return 0;
        }
    }

    int query_src, query_dst;

    if (fscanf(file, "%d %d", &query_src, &query_dst) != 2) {
        fprintf(stderr, "Error: missing Dijkstra source and destination\n");
        free_graph(graph);
        fclose(file);
        return 0;
    }

    if (query_src < 0 || query_dst < 0) {
        fprintf(stderr, "Invalid input: negative numbers are not allowed\n");
        free_graph(graph);
        fclose(file);
        return 0;
    }

    if (query_src >= n || query_dst >= n) {
        fprintf(stderr, "Error: source or destination out of range\n");
        free_graph(graph);
        fclose(file);
        return 0;
    }

    fclose(file);

    *out_graph = graph;
    *out_src = query_src;
    *out_dst = query_dst;

    return 1;
}

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
        printf("No path found\n");

#ifndef MILESTONE1
        show_graph_gui(graph, NULL, 0);
#endif

        free_graph(graph);
        return 0;
    }

    print_path(path, path_length);
    printf("%lld\n", total_weight);

#ifndef MILESTONE1
    show_graph_gui(graph, path, path_length);
#endif

    free(path);
    free_graph(graph);

    return 0;
}