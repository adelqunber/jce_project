#ifndef IPC_H
#define IPC_H

#include <sys/types.h>

#define MAX_PATH_LEN 64

typedef struct {
    int path[MAX_PATH_LEN];
    int path_length;
} PathMsg;

typedef struct {
    pid_t pid;
    int   current_node;
    int   next_node;
    int   is_done;
} TravelerMsg;

#define TRAVELER_STATE_WAITING_OUTSIDE 1
#define TRAVELER_STATE_INSIDE_NODE     2
#define TRAVELER_STATE_MOVING_EDGE     3
#define TRAVELER_STATE_DONE            4
#define TRAVELER_STATE_REQUESTING      5 /* milestone 7: requesting node access from the scheduler */

typedef struct {
    pid_t pid;
    int   traveler_index;
    int   state;
    int   current_node;
    int   next_node;
    int   is_done;
    int   remaining_weight; /* milestone 7: distance left to destination (used by SJF) */
    int   priority;         /* milestone 7: optional priority from the input file */
} SyncTravelerMsg;

#endif
