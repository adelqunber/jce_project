#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "graph.h"
#include "dijkstra.h"
#include "input_reader.h"
#include "GUI.h"
#include "IPC.h"
#include "scheduler.h"

#define EDGE_UNIT_NS 300000000L

static int write_all(int fd, const void* data, size_t size) {
    const char* p = (const char*)data;
    size_t written = 0;

    while (written < size) {
        ssize_t n = write(fd, p + written, size - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return 0;
        }
        if (n == 0) return 0;
        written += (size_t)n;
    }

    return 1;
}

static void sleep_for_ns(long nanoseconds) {
    struct timespec req;
    req.tv_sec = nanoseconds / 1000000000L;
    req.tv_nsec = nanoseconds % 1000000000L;

    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        /* keep sleeping for the remaining time after SIGCONT/EINTR */
    }
}

static int get_path_edge_weight(const Graph* graph, int src, int dst) {
    if (!graph || src < 0 || src >= graph->num_nodes) return 1;

    for (Edge* e = graph->adj[src]; e != NULL; e = e->next) {
        if (e->dst == dst) return (e->weight > 0) ? e->weight : 1;
    }

    return 1;
}

static void send_sched_msg(int fd, int traveler_index, int state,
                           int current_node, int next_node, int is_done,
                           int remaining_weight, int priority) {
    SyncTravelerMsg msg;
    msg.pid = getpid();
    msg.traveler_index = traveler_index;
    msg.state = state;
    msg.current_node = current_node;
    msg.next_node = next_node;
    msg.is_done = is_done;
    msg.remaining_weight = remaining_weight;
    msg.priority = priority;

    write_all(fd, &msg, sizeof(msg));
}

/*
 * Child process: computes its own shortest path, then for every node on the
 * path it REQUESTS access from the parent and blocks on its control pipe
 * until the parent's scheduler grants it. The child never decides node
 * order itself -- that is entirely the parent's job (see
 * show_graph_scheduled_animation in GUI.c).
 */
static void child_run_sched(const Graph* graph, int src, int dst, int priority,
                            int traveler_index, int data_fd, int control_fd) {
    int*      path        = NULL;
    int       path_length = 0;
    long long total_w     = 0;

    int result = dijkstra(graph, src, dst, &path, &path_length, &total_w);
    (void)total_w;

    PathMsg pmsg;
    memset(&pmsg, 0, sizeof(pmsg));

    if (result == 1 && path != NULL && path_length > 0 && path_length <= MAX_PATH_LEN) {
        pmsg.path_length = path_length;
        for (int i = 0; i < path_length; i++) pmsg.path[i] = path[i];
    }

    write_all(data_fd, &pmsg, sizeof(pmsg));

    if (pmsg.path_length == 0) {
        send_sched_msg(data_fd, traveler_index, TRAVELER_STATE_DONE, src, -1, 1, 0, priority);
        close(data_fd);
        close(control_fd);
        free(path);
        exit(0);
    }

    /* Suffix sums: remaining[i] = distance from path[i] to the destination.
       This is the "job length" used by SJF. */
    int* remaining = malloc((size_t)path_length * sizeof(int));
    if (remaining == NULL) {
        close(data_fd);
        close(control_fd);
        free(path);
        exit(1);
    }

    remaining[path_length - 1] = 0;
    for (int i = path_length - 2; i >= 0; i--) {
        remaining[i] = remaining[i + 1] + get_path_edge_weight(graph, path[i], path[i + 1]);
    }

    raise(SIGSTOP); /* wait here until the GUI Play button starts the run */

    for (int i = 0; i < path_length; i++) {
        int current = path[i];
        int next = (i < path_length - 1) ? path[i + 1] : -1;

        send_sched_msg(data_fd, traveler_index, TRAVELER_STATE_REQUESTING,
                       current, next, 0, remaining[i], priority);

        /* Block until the parent's scheduler grants this node. No busy
           waiting, no signals here: a simple blocking read removes any
           race between "request" and "grant". */
        char ack;
        ssize_t n;
        do {
            n = read(control_fd, &ack, 1);
        } while (n < 0 && errno == EINTR);

        send_sched_msg(data_fd, traveler_index, TRAVELER_STATE_INSIDE_NODE,
                       current, next, 0, remaining[i], priority);
        sleep(1); /* stay inside the node for 1 second, as required */

        if (i == path_length - 1) {
            send_sched_msg(data_fd, traveler_index, TRAVELER_STATE_DONE,
                           current, -1, 1, 0, priority);
            break;
        }

        send_sched_msg(data_fd, traveler_index, TRAVELER_STATE_MOVING_EDGE,
                       current, next, 0, remaining[i], priority);

        long edge_ns = (long)get_path_edge_weight(graph, current, next) * EDGE_UNIT_NS;
        sleep_for_ns(edge_ns);
    }

    free(remaining);
    close(data_fd);
    close(control_fd);
    free(path);
    exit(0);
}

static void cleanup_started_children(TravelerState* travelers, int started) {
    for (int i = 0; i < started; i++) {
        if (travelers[i].pipe_fd >= 0) {
            close(travelers[i].pipe_fd);
            travelers[i].pipe_fd = -1;
        }
        if (travelers[i].control_fd >= 0) {
            close(travelers[i].control_fd);
            travelers[i].control_fd = -1;
        }
        if (travelers[i].pid > 0) {
            kill(travelers[i].pid, SIGTERM);
            kill(travelers[i].pid, SIGCONT);
            waitpid(travelers[i].pid, NULL, 0);
        }
        free(travelers[i].path);
    }
}

static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s -schd <fcfs|sjf> <input_file>\n", prog);
}

int main(int argc, char* argv[]) {
    if (argc != 4 || strcmp(argv[1], "-schd") != 0) {
        print_usage(argv[0]);
        return 1;
    }

    SchedAlgo algo;
    if (!parse_sched_algo(argv[2], &algo)) {
        fprintf(stderr, "Error: unknown scheduling algorithm '%s' (use fcfs or sjf)\n", argv[2]);
        print_usage(argv[0]);
        return 1;
    }

    const char* filename = argv[3];

    Graph*         graph         = NULL;
    TravelerQuery* queries       = NULL;
    int            num_travelers = 0;

    if (!read_graph_file_multi(filename, &graph, &queries, &num_travelers)) {
        return 1;
    }

    TravelerState* travelers = calloc((size_t)num_travelers, sizeof(TravelerState));
    if (!travelers) {
        fprintf(stderr, "Error: memory allocation failed\n");
        free(queries);
        free_graph(graph);
        return 1;
    }

    for (int i = 0; i < num_travelers; i++) {
        travelers[i].pid         = -1;
        travelers[i].pipe_fd     = -1;
        travelers[i].control_fd  = -1;
        travelers[i].path        = NULL;
        travelers[i].path_length = 0;
    }

    for (int i = 0; i < num_travelers; i++) {
        int data_pipe[2];
        int ctrl_pipe[2];

        if (pipe(data_pipe) < 0) {
            fprintf(stderr, "Error: pipe() failed for traveler %d\n", i);
            cleanup_started_children(travelers, i);
            free(travelers); free(queries); free_graph(graph);
            return 1;
        }

        if (pipe(ctrl_pipe) < 0) {
            fprintf(stderr, "Error: pipe() failed for traveler %d\n", i);
            close(data_pipe[0]); close(data_pipe[1]);
            cleanup_started_children(travelers, i);
            free(travelers); free(queries); free_graph(graph);
            return 1;
        }

        pid_t pid = fork();

        if (pid < 0) {
            fprintf(stderr, "Error: fork() failed for traveler %d\n", i);
            close(data_pipe[0]); close(data_pipe[1]);
            close(ctrl_pipe[0]); close(ctrl_pipe[1]);
            cleanup_started_children(travelers, i);
            free(travelers); free(queries); free_graph(graph);
            return 1;
        }

        if (pid == 0) {
            close(data_pipe[0]); /* child only writes data */
            close(ctrl_pipe[1]); /* child only reads control */

            for (int j = 0; j < i; j++) {
                if (travelers[j].pipe_fd >= 0)    close(travelers[j].pipe_fd);
                if (travelers[j].control_fd >= 0) close(travelers[j].control_fd);
            }

            child_run_sched(graph, queries[i].src, queries[i].dst, queries[i].priority,
                            i, data_pipe[1], ctrl_pipe[0]);
            /* never returns */
        }

        close(data_pipe[1]); /* parent only reads data */
        close(ctrl_pipe[0]); /* parent only writes control */

        travelers[i].pid        = pid;
        travelers[i].pipe_fd    = data_pipe[0];
        travelers[i].control_fd = ctrl_pipe[1];
    }

    for (int i = 0; i < num_travelers; i++) {
        PathMsg pmsg;
        ssize_t n = read(travelers[i].pipe_fd, &pmsg, sizeof(pmsg));
        if (n == (ssize_t)sizeof(pmsg) && pmsg.path_length > 0) {
            travelers[i].path = malloc((size_t)pmsg.path_length * sizeof(int));
            if (travelers[i].path) {
                for (int k = 0; k < pmsg.path_length; k++) {
                    travelers[i].path[k] = pmsg.path[k];
                }
                travelers[i].path_length = pmsg.path_length;
            }
        }
    }

    printf("Milestone 7: scheduling algorithm = %s\n", sched_algo_name(algo));
    fflush(stdout);

    show_graph_scheduled_animation(graph, travelers, num_travelers, algo);

    for (int i = 0; i < num_travelers; i++) free(travelers[i].path);
    free(travelers);
    free(queries);
    free_graph(graph);

    return 0;
}
