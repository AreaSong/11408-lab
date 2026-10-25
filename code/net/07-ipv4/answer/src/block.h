#ifndef BLOCK_H
#define BLOCK_H

#include "ip.h"

#define BLOCK_LIST 8

typedef struct {
    int count;
    int subnet_bits;
    int host_bits;
    uint64_t usable;
    Block item[BLOCK_LIST];
} EqualSplit;

typedef struct {
    int new_prefix;
    int subnet_bits;
    uint64_t count;
    uint64_t usable;
} MaskSplit;

typedef struct {
    Block item[BLOCK_LIST];
    int count;
} VlsmSplit;

typedef struct {
    Block out;
    int exact;
} AggResult;

typedef struct {
    int possible;
    int overlap;
    int at_small;
    Block small;
    Block holes[BLOCK_LIST];
    int n_holes;
    Block added[4];
    int n_added;
} SiblingResult;

int subnet_equal(Block base, unsigned hosts, int count, EqualSplit *out, char *err, int err_cap);
int subnet_by_mask(Block base, int new_prefix, MaskSplit *out, char *err, int err_cap);
int split_min(Block base, int parts, VlsmSplit *out, char *err, int err_cap);
int aggregate_blocks(const Block *in, int n, AggResult *out, char *err, int err_cap);
int sibling_blocks(Block known, Block cand, int target, SiblingResult *out, char *err, int err_cap);

/* 已分配的是 from～to 里的主机，再去掉路由器接口，问还剩几台。 */
int hosts_left(Block net, uint32_t from, uint32_t to, uint32_t router, unsigned *used,
               uint64_t *left, char *err, int err_cap);

#endif
