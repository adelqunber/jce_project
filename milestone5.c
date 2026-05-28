#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "graph.h"
#include "dijkstra.h"
#include "input_reader.h"
#include "GUI.h"
#include "IPC.h"

static void child_run(const Graph* graph, int src, int dst, int write_fd) {
    int*      path        = NULL;
    int       path_length = 0;
    long long total_w     = 0;

    int result = dijkstra(graph, src, dst, &path, &path_length, &total_w);


    PathMsg pmsg;
    pmsg.path_length = 0;

    if (result == 1 && path != NULL && path_length > 0
            && path_length <= MAX_PATH_LEN) {
        pmsg.path_length = path_length;
        for (int i = 0; i < path_length; i++)
            pmsg.path[i] = path[i];
    }
    write(write_fd, &pmsg, sizeof(pmsg));

    if (pmsg.path_length == 0) {

        TravelerMsg msg;
        msg.pid          = getpid();
        msg.current_node = src;
        msg.next_node    = -1;
        msg.is_done      = 1;
        write(write_fd, &msg, sizeof(msg));
        close(write_fd);
        free(path);
        exit(0);
    }


    for (int i = 0; i < path_length; i++) {
        TravelerMsg msg;
        msg.pid          = getpid();
        msg.current_node = path[i];
        msg.next_node    = (i < path_length - 1) ? path[i + 1] : -1;
        msg.is_done      = (i == path_length - 1) ? 1 : 0;

        write(write_fd, &msg, sizeof(msg));

        if (i < path_length - 1)
            sleep(1);
    }

    close(write_fd);
    free(path);
    exit(0);
}

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
        int pipefd[2];

        if (pipe(pipefd) < 0) {
            fprintf(stderr, "Error: pipe() failed for traveler %d\n", i);
            for (int j = 0; j < i; j++) {
                if (travelers[j].pid > 0) {
                    kill(travelers[j].pid, SIGTERM);
                    waitpid(travelers[j].pid, NULL, 0);
                }
                if (travelers[j].pipe_fd >= 0) close(travelers[j].pipe_fd);
                free(travelers[j].path);
            }
            free(travelers); free(queries); free_graph(graph);
            return 1;
        }

        pid_t pid = fork();

        if (pid < 0) {
            fprintf(stderr, "Error: fork() failed for traveler %d\n", i);
            close(pipefd[0]); close(pipefd[1]);
            for (int j = 0; j < i; j++) {
                if (travelers[j].pid > 0) {
                    kill(travelers[j].pid, SIGTERM);
                    waitpid(travelers[j].pid, NULL, 0);
                }
                if (travelers[j].pipe_fd >= 0) close(travelers[j].pipe_fd);
                free(travelers[j].path);
            }
            free(travelers); free(queries); free_graph(graph);
            return 1;
        }

        if (pid == 0) {

            close(pipefd[0]);
            for (int j = 0; j < i; j++)
                if (travelers[j].pipe_fd >= 0) close(travelers[j].pipe_fd);
            child_run(graph, queries[i].src, queries[i].dst, pipefd[1]);

        }


        close(pipefd[1]);
        travelers[i].pid          = pid;
        travelers[i].pipe_fd      = pipefd[0];
        travelers[i].path         = NULL;
        travelers[i].path_length  = 0;
    }


    for (int i = 0; i < num_travelers; i++) {
        PathMsg pmsg;
        ssize_t n = read(travelers[i].pipe_fd, &pmsg, sizeof(pmsg));
        if (n == (ssize_t)sizeof(pmsg) && pmsg.path_length > 0) {
            travelers[i].path = malloc(pmsg.path_length * sizeof(int));
            if (travelers[i].path) {
                for (int k = 0; k < pmsg.path_length; k++)
                    travelers[i].path[k] = pmsg.path[k];
                travelers[i].path_length = pmsg.path_length;
            }
        }
    }


    show_graph_multi_animation(graph, travelers, num_travelers);


    for (int i = 0; i < num_travelers; i++) free(travelers[i].path);
    free(travelers);
    free(queries);
    free_graph(graph);

    return 0;
}