#ifndef BP_H
#define BP_H

#include <stdio.h>

#define BP_CAP 18
#define BP_MAX_N 256

enum { BP_OK = 0, BP_DUP = 1, BP_MISS = 2, BP_RANGE = 3 };

typedef struct BpNode {
    int n;
    int leaf;
    int key[BP_CAP];
    struct BpNode *child[BP_CAP];
    struct BpNode *parent;
    struct BpNode *next;
} BpNode;

typedef struct BpTree {
    int order;
    BpNode *root;
    int key_count;
    int node_count;
} BpTree;

BpTree *bp_create(int order);
void bp_free(BpTree *tree);

int bp_insert(BpTree *tree, int key, FILE *tr);
int bp_find(const BpTree *tree, int key, FILE *tr);
int bp_valid(const BpTree *tree);
int bp_height(const BpTree *tree);
int bp_leaves(const BpTree *tree, int *out, int cap);
void bp_report(const BpTree *tree, FILE *out);

#endif
