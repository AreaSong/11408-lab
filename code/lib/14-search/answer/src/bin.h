#ifndef BIN_H
#define BIN_H

#include "asl.h"
#include "tab.h"

#include <stdio.h>

/*
 * 折半查找。表必须严格升序，下标从 1 起。
 * ceil_mid 为 0 时 mid = (low+high)/2，这是王道的整除；
 * 为 1 时 mid = (low+high+1)/2，整棵树换成镜像，ASL 不变。
 */
typedef struct {
    int n;
    int ceil_mid;
    int root;
    int height;
    int weighted;
    int key[TABLE_MAX + 1];
    int weight[TABLE_MAX + 1];
    int depth[TABLE_MAX + 1];
    int left[TABLE_MAX + 1];
    int right[TABLE_MAX + 1];
} BinTable;

int bin_mode(const char *name);
int bin_load(BinTable *t, int ceil_mid, const int *keys, int n);
int bin_weight(BinTable *t, const int *w, int n);
int bin_bound(int n);
int bin_tree_sum(int n, int ceil_mid, long long *sum, int *height);
int bin_fill_depth(int n, int ceil_mid, int *depth);
void bin_asl(const BinTable *t, AslPair *a);
void bin_print_asl(const BinTable *t, FILE *out);
void bin_print_tree(const BinTable *t, FILE *out);
void bin_report(const BinTable *t, FILE *out);
int bin_find(const BinTable *t, int key, FILE *out, int *cmp);
int bin_path(const int *keys, int n, FILE *out);
int bin_suitable(const char *store, const char *ord);
void bin_fit(const char *store, const char *ord, FILE *out);
int bin_worst(int n, int ceil_mid, FILE *out);

#endif
