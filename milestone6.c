#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "graph.h"
#include "dijkstra.h"
#include "input_reader.h"
#include "GUI.h"
#include "IPC.h"

#define EDGE_UNIT_NS       300000000L
#define MIN_VISUAL_EXIT_NS 150000000L
#define MAX_VISUAL_EXIT_NS 500000000L

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
        /* Continue sleeping for the remaining time after SIGCONT/EINTR. */
    }
}

static int get_edge_weight_for_sleep(const Graph* graph, int src, int dst) {
    int weight = 1;

    if (graph != NULL && src >= 0 && src < graph->num_nodes) {
        for (Edge* e = graph->adj[src]; e != NULL; e = e->next) {
            if (e->dst == dst) {
                weight = (e->weight > 0) ? e->weight : 1;
                break;
            }
        }
    }

    return weight;
}

static long get_edge_time_ns(const Graph* graph, int src, int dst) {
    int weight = get_edge_weight_for_sleep(graph, src, dst);
    return (long)weight * EDGE_UNIT_NS;
}

static long get_visual_exit_time_ns(const Graph* graph, int src, int dst) {
    int weight = get_edge_weight_for_sleep(graph, src, dst);
    long exit_time = (long)weight * 45000000L;
    long edge_time = (long)weight * EDGE_UNIT_NS;

    if (exit_time < MIN_VISUAL_EXIT_NS) exit_time = MIN_VISUAL_EXIT_NS;
    if (exit_time > MAX_VISUAL_EXIT_NS) exit_time = MAX_VISUAL_EXIT_NS;
    if (exit_time > edge_time) exit_time = edge_time;

    return exit_time;
}

static void send_sync_msg(int write_fd,
                          int traveler_index,
                          int state,
                          int current_node,
                          int next_node,
                          int is_done) {
    SyncTravelerMsg msg;
    msg.pid = getpid();
    msg.traveler_index = traveler_index;
    msg.state = state;
    msg.current_node = current_node;
    msg.next_node = next_node;
    msg.is_done = is_done;

    write_all(write_fd, &msg, sizeof(msg));
}

static int wait_on_node_lock(sem_t* node_locks, int node) {
    while (sem_wait(&node_locks[node]) == -1) {
        if (errno == EINTR) continue;
        return 0;
    }
    return 1;
}

/*
 * Acquire a node correctly:
 * - If the node semaphore is free, enter immediately and do NOT send WAITING_OUTSIDE.
 * - If the node semaphore is busy, send WAITING_OUTSIDE once, then block on sem_wait.
 *
 * wait_from/wait_to describe where the traveler waits visually:
 * - For normal nodes after an edge: wait_from = previous node, wait_to = node being entered.
 * - For the first node: wait_from = first node, wait_to = -1.
 */
static int acquire_node_or_wait(sem_t* node_locks,
                                int node,
                                int write_fd,
                                int traveler_index,
                                int wait_from,
                                int wait_to) {
    while (sem_trywait(&node_locks[node]) == -1) {
        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN) {
            send_sync_msg(write_fd,
                          traveler_index,
                          TRAVELER_STATE_WAITING_OUTSIDE,
                          wait_from,
                          wait_to,
                          0);

            return wait_on_node_lock(node_locks, node);
        }

        return 0;
    }

    return 1;
}

static void child_run_sync(const Graph* graph,
                           int src,
                           int dst,
                           int traveler_index,
                           int write_fd,
                           sem_t* node_locks) {
    int*      path        = NULL;
    int       path_length = 0;
    long long total_w     = 0;

    int result = dijkstra(graph, src, dst, &path, &path_length, &total_w);
    (void)total_w;

    PathMsg pmsg;
    memset(&pmsg, 0, sizeof(pmsg));

    if (result == 1 && path != NULL && path_length > 0 && path_length <= MAX_PATH_LEN) {
        pmsg.path_length = path_length;
        for (int i = 0; i < path_length; i++) {
            pmsg.path[i] = path[i];
        }
    }

    write_all(write_fd, &pmsg, sizeof(pmsg));

    if (pmsg.path_length == 0) {
        send_sync_msg(write_fd, traveler_index, TRAVELER_STATE_DONE, src, -1, 1);
        close(write_fd);
        free(path);
        exit(0);
    }

    /* Stop here so the GUI can start all children together when Play is pressed. */
    raise(SIGSTOP);

    for (int i = 0; i < path_length; i++) {
        int current = path[i];
        int previous = (i > 0) ? path[i - 1] : current;
        int next = (i < path_length - 1) ? path[i + 1] : -1;

        int wait_from = previous;
        int wait_to = (i > 0) ? current : -1;

        if (!acquire_node_or_wait(node_locks,
                                  current,
                                  write_fd,
                                  traveler_index,
                                  wait_from,
                                  wait_to)) {
            send_sync_msg(write_fd, traveler_index, TRAVELER_STATE_DONE, current, -1, 1);
            close(write_fd);
            free(path);
            exit(1);
        }

        /* Critical section starts here: this traveler is the only one inside current. */
        send_sync_msg(write_fd,
                      traveler_index,
                      TRAVELER_STATE_INSIDE_NODE,
                      current,
                      next,
                      0);

        /* Assignment requirement: stay inside the node for one full second. */
        sleep(1);

        if (i == path_length - 1) {
            send_sync_msg(write_fd,
                          traveler_index,
                          TRAVELER_STATE_DONE,
                          current,
                          -1,
                          1);

            sem_post(&node_locks[current]);
            break;
        }

        /* Leave the current node and start moving to the next node. */
        send_sync_msg(write_fd,
                      traveler_index,
                      TRAVELER_STATE_MOVING_EDGE,
                      current,
                      next,
                      0);

        /* Keep the current node locked while the traveler visually clears the
           node circle.  The delay is counted as part of the edge movement, so
           the traveler does not spend extra fake time before the next node. */
        long edge_time_ns = get_edge_time_ns(graph, current, next);
        long visual_exit_ns = get_visual_exit_time_ns(graph, current, next);

        sleep_for_ns(visual_exit_ns);
        sem_post(&node_locks[current]);

        if (edge_time_ns > visual_exit_ns) {
            sleep_for_ns(edge_time_ns - visual_exit_ns);
        }
    }

    close(write_fd);
    free(path);
    exit(0);
}

static void cleanup_started_children(TravelerState* travelers, int started) {
    for (int i = 0; i < started; i++) {
        if (travelers[i].pipe_fd >= 0) {
            close(travelers[i].pipe_fd);
            travelers[i].pipe_fd = -1;
        }
        if (travelers[i].pid > 0) {
            kill(travelers[i].pid, SIGTERM);
            kill(travelers[i].pid, SIGCONT);
            waitpid(travelers[i].pid, NULL, 0);
        }
        free(travelers[i].path);
    }
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

    sem_t* node_locks = mmap(NULL,
                             (size_t)graph->num_nodes * sizeof(sem_t),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS,
                             -1,
                             0);

    if (node_locks == MAP_FAILED) {
        perror("mmap");
        free(queries);
        free_graph(graph);
        return 1;
    }

    for (int i = 0; i < graph->num_nodes; i++) {
        if (sem_init(&node_locks[i], 1, 1) == -1) {
            perror("sem_init");
            for (int j = 0; j < i; j++) sem_destroy(&node_locks[j]);
            munmap(node_locks, (size_t)graph->num_nodes * sizeof(sem_t));
            free(queries);
            free_graph(graph);
            return 1;
        }
    }

    TravelerState* travelers = calloc((size_t)num_travelers, sizeof(TravelerState));
    if (!travelers) {
        fprintf(stderr, "Error: memory allocation failed\n");
        for (int i = 0; i < graph->num_nodes; i++) sem_destroy(&node_locks[i]);
        munmap(node_locks, (size_t)graph->num_nodes * sizeof(sem_t));
        free(queries);
        free_graph(graph);
        return 1;
    }

    for (int i = 0; i < num_travelers; i++) {
        travelers[i].pid = -1;
        travelers[i].pipe_fd = -1;
        travelers[i].path = NULL;
        travelers[i].path_length = 0;
    }

    for (int i = 0; i < num_travelers; i++) {
        int pipefd[2];

        if (pipe(pipefd) < 0) {
            perror("pipe");
            cleanup_started_children(travelers, i);
            free(travelers);
            for (int j = 0; j < graph->num_nodes; j++) sem_destroy(&node_locks[j]);
            munmap(node_locks, (size_t)graph->num_nodes * sizeof(sem_t));
            free(queries);
            free_graph(graph);
            return 1;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            close(pipefd[0]);
            close(pipefd[1]);
            cleanup_started_children(travelers, i);
            free(travelers);
            for (int j = 0; j < graph->num_nodes; j++) sem_destroy(&node_locks[j]);
            munmap(node_locks, (size_t)graph->num_nodes * sizeof(sem_t));
            free(queries);
            free_graph(graph);
            return 1;
        }

        if (pid == 0) {
            close(pipefd[0]);
            for (int j = 0; j < i; j++) {
                if (travelers[j].pipe_fd >= 0) close(travelers[j].pipe_fd);
            }
            child_run_sync(graph,
                           queries[i].src,
                           queries[i].dst,
                           i,
                           pipefd[1],
                           node_locks);
        }

        close(pipefd[1]);
        travelers[i].pid = pid;
        travelers[i].pipe_fd = pipefd[0];
    }

    for (int i = 0; i < num_travelers; i++) {
        PathMsg pmsg;
        ssize_t n = read(travelers[i].pipe_fd, &pmsg, sizeof(pmsg));
        if (n == (ssize_t)sizeof(pmsg) && pmsg.path_length > 0) {
            travelers[i].path = malloc((size_t)pmsg.path_length * sizeof(int));
            if (travelers[i].path != NULL) {
                for (int k = 0; k < pmsg.path_length; k++) {
                    travelers[i].path[k] = pmsg.path[k];
                }
                travelers[i].path_length = pmsg.path_length;
            }
        }
    }

    show_graph_synchronized_animation(graph, travelers, num_travelers);

    for (int i = 0; i < num_travelers; i++) {
        free(travelers[i].path);
    }
    free(travelers);

    for (int i = 0; i < graph->num_nodes; i++) {
        sem_destroy(&node_locks[i]);
    }
    munmap(node_locks, (size_t)graph->num_nodes * sizeof(sem_t));

    free(queries);
    free_graph(graph);

    return 0;
}
