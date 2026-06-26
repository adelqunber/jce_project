#include <stdio.h>
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

    return 0;
}

const char* sched_algo_name(SchedAlgo algo) {
    switch (algo) {
        case SCHED_FCFS: return "FCFS (First-Come-First-Served)";
        case SCHED_SJF:  return "SJF (Shortest-Job-First)";
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
        } else {
            /* SCHED_FCFS and any future default: arrival order. */
            better = entries[i].arrival_seq < entries[best].arrival_seq;
        }

        if (better) {
            best = i;
        }
    }

    /* Print who is waiting, who was selected, and why. */
    printf("[SCHED/%s] waiting (%d): ",
           algo == SCHED_SJF ? "SJF" : "FCFS", count);
    for (int i = 0; i < count; i++) {
        if (algo == SCHED_SJF)
            printf("T%d(rem=%d)", entries[i].traveler_index, entries[i].remaining_weight);
        else
            printf("T%d(seq=%ld)", entries[i].traveler_index, entries[i].arrival_seq);
        if (i < count - 1) printf(", ");
    }
    if (algo == SCHED_SJF)
        printf(" -> T%d selected (shortest remaining distance: %d)\n",
               entries[best].traveler_index, entries[best].remaining_weight);
    else
        printf(" -> T%d selected (earliest arrival: seq=%ld)\n",
               entries[best].traveler_index, entries[best].arrival_seq);
    fflush(stdout);

    return best;
}
