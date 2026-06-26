#ifndef SCHEDULER_H
#define SCHEDULER_H

/*
 * Milestone 7: scheduling algorithms for intersection (node) access.
 *
 * Three algorithms are supported, selectable on the command line with
 * "-schd <name>":
 *
 *   fcfs  First-Come-First-Served: travelers enter the node in the order
 *         they requested it.
 *
 *   sjf   Shortest-Job-First: among the travelers waiting for a node, the
 *         one with the smallest remaining distance to its destination is
 *         let in first (ties broken by arrival order).
 *
 *   priority  The traveler process with the smallest PID enters first.
 *             Ties are broken by arrival order.
 */

typedef enum {
    SCHED_FCFS     = 0,
    SCHED_SJF      = 1,
    SCHED_PRIORITY = 2
} SchedAlgo;

/* One traveler waiting in a node's queue. */
typedef struct {
    int    traveler_index;
    int    process_id;        /* child PID, used by Priority scheduling */
    long   arrival_seq;       /* used by FCFS, and as a tie-breaker for SJF */
    int    remaining_weight;  /* distance left to destination, used by SJF */
    int    priority;          /* optional data from the input file */
    double request_time;      /* used only to report waiting time, informational */
} WaitEntry;

/* Parses "fcfs", "sjf", or "priority". Returns 1 on success, 0 if unknown. */
int parse_sched_algo(const char* name, SchedAlgo* out_algo);

/* Human readable name shown in the GUI / logs. */
const char* sched_algo_name(SchedAlgo algo);

/* Returns the index (within entries[0..count-1]) of the entry that should be
 * granted access next, according to the given algorithm. Returns -1 if
 * count <= 0. */
int sched_pick_index(SchedAlgo algo, const WaitEntry* entries, int count);

#endif
