#ifndef BEST_H
#define BEST_H

#include <stdio.h>

/* k 叉哈夫曼树。段长短的先归并，总的记录读写次数是 2×WPL。 */

#define BEST_WAYS 8
#define BEST_RUNS 16
#define BEST_STEPS 32
#define BEST_LEN_MAX 1000000LL

typedef struct {
    int k;
    int runs;
    int dummy;
    int steps;
    long long part[BEST_STEPS][BEST_WAYS];
    long long made[BEST_STEPS];
    long long wpl;
    long long io;
    long long cmp_worst; /* 仅 k = 2 有意义：每次合并的最坏关键字比较是两端长度之和减 1 */
} BestTree;

int best_tree(int ways, const long long *len, int runs, BestTree *out);
void best_print(const BestTree *tree, FILE *out);

#endif
