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

typedef struct {
    pid_t pid;
    int   traveler_index;
    int   state;
    int   current_node;
    int   next_node;
    int   is_done;
} SyncTravelerMsg;

#endif
