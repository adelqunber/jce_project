#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#include "graph.h"
#include "dijkstra.h"
#include "input_reader.h"
#include "GUI.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    Graph*         graph         = NULL;
    TravelerQuery* queries       = NULL;
    int            num_travelers = 0;

    if (!read_graph_file_multi(argv[1], &graph, &queries, &num_travelers)) {
        return 1;
    }

    TravelerState* travelers = calloc(num_travelers, sizeof(TravelerState));
    if (!travelers) {
        fprintf(stderr, "Error: memory allocation failed\n");
        free(queries);
        free_graph(graph);
        return 1;
    }

    for (int i = 0; i < num_travelers; i++) {
        int*      path        = NULL;
        int       path_length = 0;
        long long total_w     = 0;

        int result = dijkstra(graph, queries[i].src, queries[i].dst,
                              &path, &path_length, &total_w);

        if (result == 1) {
            travelers[i].path        = path;
            travelers[i].path_length = path_length;
        } else {
            travelers[i].path        = NULL;
            travelers[i].path_length = 0;
        }
        travelers[i].pid     = -1;
        travelers[i].pipe_fd = -1;
    }

    for (int i = 0; i < num_travelers; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            fprintf(stderr, "Error: fork() failed for traveler %d\n", i);
            for (int j = 0; j < i; j++) {
                if (travelers[j].pid > 0) {
                    kill(travelers[j].pid, SIGTERM);
                }
            }
            for (int j = 0; j < num_travelers; j++) free(travelers[j].path);
            free(travelers);
            free(queries);
            free_graph(graph);
            return 1;
        }

        if (pid == 0) {
            printf("[%d] started\n", (int)getpid());
            fflush(stdout);

            while (1) {
                sleep(1);
            }
        }
        travelers[i].pid = pid;
    }

    show_graph_multi_animation(graph, travelers, num_travelers);

    for (int i = 0; i < num_travelers; i++) free(travelers[i].path);
    free(travelers);
    free(queries);
    free_graph(graph);

    return 0;
}