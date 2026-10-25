#include "formula.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

int k_ary_max(int ways, int height, long long *out) {
    long long sum = 1;
    long long term = 1;
    if (ways < 1 || height < 1 || out == NULL) {
        return THR_RANGE;
    }
    if (ways == 1) {
        *out = height;
        return THR_OK;
    }
    for (int level = 1; level < height; level++) {
        if (term > LLONG_MAX / ways) {
            return THR_SIZE;
        }
        term *= ways;
        if (sum > LLONG_MAX - term) {
            return THR_SIZE;
        }
        sum += term;
    }
    *out = sum;
    return THR_OK;
}

static int cmp_deg(const void *a, const void *b) {
    const DegItem *x = a;
    const DegItem *y = b;
    return x->deg - y->deg;
}

int degree_print(const DegItem *items, int nitem, FILE *out) {
    DegItem copy[21];
    int seen[21];
    long long n0 = 1;
    long long given = 0;
    long long branches = 0;
    if (nitem < 1 || nitem > 20) {
        return THR_FORMAT;
    }
    memset(seen, 0, sizeof seen);
    for (int i = 0; i < nitem; i++) {
        if (items[i].deg < 1 || items[i].deg > 20 || items[i].count < 0) {
            return THR_RANGE;
        }
        if (seen[items[i].deg]) {
            return THR_DUP;
        }
        seen[items[i].deg] = 1;
        copy[i] = items[i];
    }
    qsort(copy, (size_t)nitem, sizeof copy[0], cmp_deg);
    fprintf(out, "degree");
    for (int i = 0; i < nitem; i++) {
        long long part = (long long)(copy[i].deg - 1) * copy[i].count;
        fprintf(out, " n%d=%d", copy[i].deg, copy[i].count);
        given += copy[i].count;
        branches += (long long)copy[i].deg * copy[i].count;
        n0 += part;
    }
    fprintf(out, "\n");
    fprintf(out, "sum_deg=%lld\n", branches);
    fprintf(out, "n0=1");
    for (int i = 0; i < nitem; i++) {
        fprintf(out, "+%d*n%d", copy[i].deg - 1, copy[i].deg);
    }
    fprintf(out, "=%lld\n", n0);
    fprintf(out, "branch_nodes=%lld n=%lld edges=%lld\n", given, n0 + given, branches);
    return THR_OK;
}

int edges_print(int nodes, int edges, FILE *out) {
    int trees;
    if (nodes < 1 || edges < 0) {
        return THR_RANGE;
    }
    trees = nodes - edges;
    fprintf(out, "edges n=%d e=%d\n", nodes, edges);
    if (trees < 1) {
        fprintf(out, "status=reject trees=%d\n", trees);
        return THR_OK;
    }
    fprintf(out, "trees=n-e=%d\n", trees);
    fprintf(out, "one_tree_edges=%d\n", nodes - 1);
    return THR_OK;
}

int noright_print(int nodes, int leaves, FILE *out) {
    int branch;
    if (nodes < 1 || leaves < 1 || leaves > nodes) {
        return THR_RANGE;
    }
    branch = nodes - leaves;
    fprintf(out, "noright n=%d n0=%d\n", nodes, leaves);
    fprintf(out, "branch=%d\n", branch);
    fprintf(out, "right_null=branch+1=%d\n", branch + 1);
    fprintf(out, "with_right=n0-1=%d\n", leaves - 1);
    return THR_OK;
}

int height_print(int ways, int nodes, FILE *out) {
    int height = 0;
    long long full = 0;
    if (ways < 1 || nodes < 1) {
        return THR_RANGE;
    }
    fprintf(out, "height k=%d n=%d leaf_height=1\n", ways, nodes);
    for (int h = 1; h <= 31; h++) {
        int err = k_ary_max(ways, h, &full);
        if (err != THR_OK) {
            return err;
        }
        if (full >= nodes) {
            height = h;
            break;
        }
    }
    if (height == 0) {
        return THR_SIZE;
    }
    if (height > 1) {
        k_ary_max(ways, height - 1, &full);
        fprintf(out, "full h=%d max=%lld\n", height - 1, full);
    }
    k_ary_max(ways, height, &full);
    fprintf(out, "full h=%d max=%lld\n", height, full);
    fprintf(out, "min=%d\n", height);
    return THR_OK;
}
