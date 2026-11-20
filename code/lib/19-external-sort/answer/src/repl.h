#ifndef REPL_H
#define REPL_H

#include "ext.h"

#include <stdio.h>

/* 置换-选择：工作区里不小于刚输出关键字的最小值继续写进当前段，否则另起一段。 */

#define REPL_M 8
#define REPL_N 32
#define REPL_RUNS 32

typedef struct {
    int n;
    int runs;
    int first;
    int bound_max;
    int bound_min;
    int len[REPL_RUNS];
    int key[REPL_N];
    int ord[REPL_N];
} Repl;

int repl_select(int memory, const int *in, int n, Repl *out);
void repl_print(const Repl *result, FILE *out);

#endif
