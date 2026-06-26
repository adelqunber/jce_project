#include <string.h>
#include "scheduler.h"

int parse_sched_algo(const char* name, SchedAlgo* out_algo) {
    if (name == NULL || out_algo == NULL) {
        return 0;
    }

    if (strcmp(name, "fcfs") == 0) {
        *out_algo = SCHED_FCFS;
        return 1;
    }

    if (strcmp(name, "sjf") == 0) {
        *out_algo = SCHED_SJF;
        return 1;
    }

    if (strcmp(name, "priority") == 0) {
        *out_algo = SCHED_PRIORITY;
        return 1;
    }

    return 0;
}

const char* sched_algo_name(SchedAlgo algo) {
    switch (algo) {
        case SCHED_FCFS: return "FCFS (First-Come-First-Served)";
        case SCHED_SJF:  return "SJF (Shortest-Job-First)";
        case SCHED_PRIORITY: return "Priority (Lowest PID First)";
        default:         return "Unknown";
    }
}

int sched_pick_index(SchedAlgo algo, const WaitEntry* entries, int count) {
    if (entries == NULL || count <= 0) {
        return -1;
    }

    int best = 0;

    for (int i = 1; i < count; i++) {
        int better;

        if (algo == SCHED_SJF) {
            if (entries[i].remaining_weight != entries[best].remaining_weight) {
                better = entries[i].remaining_weight < entries[best].remaining_weight;
            } else {
                better = entries[i].arrival_seq < entries[best].arrival_seq;
            }
        } else if (algo == SCHED_PRIORITY) {
            if (entries[i].pid != entries[best].pid) {
                better = entries[i].pid < entries[best].pid;
            } else {
                better = entries[i].arrival_seq < entries[best].arrival_seq;
            }
        } else {
            /* SCHED_FCFS and any future default: arrival order. */
            better = entries[i].arrival_seq < entries[best].arrival_seq;
        }

        if (better) {
            best = i;
        }
    }

    return best;
}
