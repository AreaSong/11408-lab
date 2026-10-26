#ifndef FACT_H
#define FACT_H

#include <stdio.h>

typedef struct {
    int edges;
    int below;
    const int *deg;
    const int *cnt;
    int npairs;
} LeastIn;

int fact_bound(int n, FILE *out);

int fact_bound_check(int n, FILE *out);

int fact_enum(int n, FILE *out);

void fact_least(const LeastIn *in, FILE *out);

#endif
