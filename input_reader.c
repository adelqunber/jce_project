#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input_reader.h"
#include "graph.h"

#define SEPARATOR "---"
#define LINE_BUF  256

static int seek_to_separator(FILE* file) {
    char line[LINE_BUF];

    while (fgets(line, sizeof(line), file) != NULL) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
                           || line[len-1] == ' ' || line[len-1] == '\t')) {
            line[--len] = '\0';
        }

        if (strcmp(line, SEPARATOR) == 0) {
            return 1;
        }
    }

    return 0;
}

static Graph* read_graph_edges(FILE* file, int* out_n) {
    int n, m;

    if (fscanf(file, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error: invalid file format\n");
        return NULL;
    }

    if (n < 0 || m < 0) {
        fprintf(stderr, "Invalid input: negative numbers are not allowed\n");
        return NULL;
    }

    if (n == 0) {
        fprintf(stderr, "Error: number of nodes must be positive\n");
        return NULL;
    }

    Graph* graph = create_graph(n);
    if (graph == NULL) {
        fprintf(stderr, "Error: memory allocation failed\n");
        return NULL;
    }

    for (int i = 0; i < m; i++) {
        int src, dst, weight;

        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            fprintf(stderr, "Error: invalid edge format\n");
            free_graph(graph);
            return NULL;
        }

        if (src < 0 || dst < 0 || weight < 0) {
            fprintf(stderr, "Invalid input: negative numbers are not allowed\n");
            free_graph(graph);
            return NULL;
        }

        if (src >= n || dst >= n) {
            fprintf(stderr, "Error: node index out of range\n");
            free_graph(graph);
            return NULL;
        }

        if (!add_edge(graph, src, dst, weight)) {
            fprintf(stderr, "Error: failed to add edge\n");
            free_graph(graph);
            return NULL;
        }
    }

    *out_n = n;
    return graph;
}

int read_graph_file(
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

    int n;
    Graph* graph = read_graph_edges(file, &n);
    if (graph == NULL) {
        fclose(file);
        return 0;
    }

    long pos_after_edges = ftell(file);

    int found_sep = seek_to_separator(file);

    if (!found_sep) {
        fseek(file, pos_after_edges, SEEK_SET);
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
    *out_src   = query_src;
    *out_dst   = query_dst;

    return 1;
}

int read_graph_file_multi(
    const char* filename,
    Graph** out_graph,
    TravelerQuery** out_travelers,
    int* out_num_travelers
) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: could not open file\n");
        return 0;
    }

    int n;
    Graph* graph = read_graph_edges(file, &n);
    if (graph == NULL) {
        fclose(file);
        return 0;
    }

    if (!seek_to_separator(file)) {
        fprintf(stderr, "Error: missing first '---' separator\n");
        free_graph(graph);
        fclose(file);
        return 0;
    }

    if (!seek_to_separator(file)) {
        fprintf(stderr, "Error: missing second '---' separator\n");
        free_graph(graph);
        fclose(file);
        return 0;
    }

    int num_travelers;
    if (fscanf(file, "%d", &num_travelers) != 1 || num_travelers <= 0) {
        fprintf(stderr, "Error: invalid number of travelers\n");
        free_graph(graph);
        fclose(file);
        return 0;
    }

    TravelerQuery* travelers = malloc(num_travelers * sizeof(TravelerQuery));
    if (travelers == NULL) {
        fprintf(stderr, "Error: memory allocation failed\n");
        free_graph(graph);
        fclose(file);
        return 0;
    }

    for (int i = 0; i < num_travelers; i++) {
        int src, dst;
        if (fscanf(file, "%d %d", &src, &dst) != 2) {
            fprintf(stderr, "Error: invalid traveler format at index %d\n", i);
            free(travelers);
            free_graph(graph);
            fclose(file);
            return 0;
        }

        if (src < 0 || dst < 0 || src >= n || dst >= n) {
            fprintf(stderr, "Error: traveler %d node out of range\n", i);
            free(travelers);
            free_graph(graph);
            fclose(file);
            return 0;
        }

        travelers[i].src = src;
        travelers[i].dst = dst;
    }

    fclose(file);

    *out_graph         = graph;
    *out_travelers     = travelers;
    *out_num_travelers = num_travelers;

    return 1;
}