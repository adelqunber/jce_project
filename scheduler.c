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

//_________________
// Task B :
/* Print every scheduling decision to standard output so the waiting queue,
 * selected traveler, and selection reason can be followed during runtime. */
static void print_sched_decision(SchedAlgo algo,
                                 const WaitEntry* entries,
                                 int count,
                                 int selected_index) {
    const WaitEntry* selected = &entries[selected_index];

    printf("\n[SCHEDULER] Decision using %s\n", sched_algo_name(algo));
    printf("Waiting travelers:");

    for (int i = 0; i < count; i++) {
        printf(" T%d(arrival=%ld, remaining=%d)",
               entries[i].traveler_index,
               entries[i].arrival_seq,
               entries[i].remaining_weight);

        if (i < count - 1) {
            printf(",");
        }
    }

    printf("\nSelected: T%d\n", selected->traveler_index);

    if (algo == SCHED_SJF) {
        int shortest_count = 0;

        for (int i = 0; i < count; i++) {
            if (entries[i].remaining_weight == selected->remaining_weight) {
                shortest_count++;
            }
        }

        if (shortest_count > 1) {
            printf("Reason: shortest remaining distance (%d); "
                   "tie broken by earliest arrival (%ld).\n",
                   selected->remaining_weight,
                   selected->arrival_seq);
        } else {
            printf("Reason: shortest remaining distance (%d).\n",
                   selected->remaining_weight);
        }
    } else {
        printf("Reason: earliest arrival (%ld), according to FCFS.\n",
               selected->arrival_seq);
    }

    fflush(stdout);
}


// ______________________
// Finished task B .

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

    print_sched_decision(algo, entries, count, best);
    return best;
}
