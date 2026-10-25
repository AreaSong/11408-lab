#ifndef WALK_H
#define WALK_H

#include "fields.h"

#include <stdint.h>
#include <stdio.h>

enum {
    WALK_ROW = 0,
    WALK_COL = 1,
    WALK_BOTH = 2
};

typedef struct {
    int rows;
    int cols;
    int elem_bytes;
    uint64_t base;
    int addr_bits;
    int block_bytes;
    int n_lines;
    int n_ways;
    int replace;
    int write_back;
    int write_alloc;
    int update; /* 1：每个元素先读后写；0：只读 */
} WalkIn;

typedef struct {
    int hits;
    int misses;
    int accesses;
} WalkOut;

int walk_run(const WalkIn *in, int col_major, WalkOut *out);
int show_walk(FILE *out, const WalkIn *in, int which);

#endif
