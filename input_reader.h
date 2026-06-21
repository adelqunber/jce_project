#ifndef INPUT_READER_H
#define INPUT_READER_H

#include "graph.h"

int read_graph_file(
    const char* filename,
    Graph** out_graph,
    int* out_src,
    int* out_dst
);

typedef struct {
    int src;
    int dst;
    int priority; /* milestone 7: optional, defaults to 0 if absent from the input file */
} TravelerQuery;

int read_graph_file_multi(
    const char* filename,
    Graph** out_graph,
    TravelerQuery** out_travelers,
    int* out_num_travelers
);

#endif