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

#endif