#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "dijkstra.h"

#define INF (LLONG_MAX / 4)

static int find_min_distance_node(
    const long long* dist,
    const int* visited,
    int num_nodes
) {
    long long best_distance = INF;
    int best_node = -1;

    for (int i = 0; i < num_nodes; i++) {
        if (!visited[i] && dist[i] < best_distance) {
            best_distance = dist[i];
            best_node = i;
        }
    }

    return best_node;
}

int dijkstra(
    const Graph* graph,
    int src,
    int dst,
    int** out_path,
    int* out_path_length,
    long long* out_total_weight
) {
    if (graph == NULL || out_path == NULL || out_path_length == NULL || out_total_weight == NULL) {
        return -1;
    }

    int n = graph->num_nodes;

    if (src < 0 || dst < 0 || src >= n || dst >= n) {
        return -1;
    }

    long long* dist = malloc(n * sizeof(long long));
    int* visited = calloc(n, sizeof(int));
    int* parent = malloc(n * sizeof(int));

    if (dist == NULL || visited == NULL || parent == NULL) {
        free(dist);
        free(visited);
        free(parent);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        dist[i] = INF;
        parent[i] = -1;
    }

    dist[src] = 0;

    for (int i = 0; i < n; i++) {
        int u = find_min_distance_node(dist, visited, n);

        if (u == -1) {
            break;
        }

        visited[u] = 1;

        if (u == dst) {
            break;
        }

        Edge* edge = graph->adj[u];

        while (edge != NULL) {
            int v = edge->dst;
            int weight = edge->weight;

            if (!visited[v] && dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                parent[v] = u;
            }

            edge = edge->next;
        }
    }

    if (dist[dst] == INF) {
        free(dist);
        free(visited);
        free(parent);
        return 0;
    }

    int count = 0;
    int current = dst;

    while (current != -1) {
        count++;
        current = parent[current];
    }

    int* path = malloc(count * sizeof(int));
    if (path == NULL) {
        free(dist);
        free(visited);
        free(parent);
        return -1;
    }

    current = dst;

    for (int i = count - 1; i >= 0; i--) {
        path[i] = current;
        current = parent[current];
    }

    *out_path = path;
    *out_path_length = count;
    *out_total_weight = dist[dst];

    free(dist);
    free(visited);
    free(parent);

    return 1;
}

void print_path(const int* path, int path_length) {
    for (int i = 0; i < path_length; i++) {
        printf("%d", path[i]);

        if (i < path_length - 1) {
            printf(" -> ");
        }
    }

    printf("\n");
}