#ifndef INPUT_READER_H
#define INPUT_READER_H

#include "graph.h"

int read_graph_file(
    const char* filename,
    Graph** out_graph,
    int* out_src,
    int* out_dst
);

#endif