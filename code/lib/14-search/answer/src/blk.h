#ifndef BLK_H
#define BLK_H

#include "tab.h"

#include <stdio.h>

/*
 * 分块查找。块间有序：后一块的每个关键字都大于前一块的最大关键字。
 * 块内顺序时块内可以乱；块内折半时这一块必须严格升序。
 * 索引和块内的比较次数分开加。折半固定向下取整。
 */
#define BLOCK_MAX 16
#define BLOCK_LEN 32

typedef struct {
    int index_bin;
    int inner_bin;
    int b;
    int cnt[BLOCK_MAX + 1];
    int key[BLOCK_MAX + 1][BLOCK_LEN + 1];
    int mink[BLOCK_MAX + 1];
    int maxk[BLOCK_MAX + 1];
} BlkTable;

typedef struct {
    long long exact_n;
    long long exact_d;
    long long index_n;
    long long index_d;
    long long inner_n;
    long long inner_d;
    /* rough_d 为 0：块长不齐，王道那条等长公式用不上。 */
    long long rough_n;
    long long rough_d;
} BlkAsl;

int blk_mode(const char *name);
void blk_begin(BlkTable *t, int index_bin, int inner_bin);
int blk_add(BlkTable *t, const int *keys, int n);
void blk_show(const BlkTable *t, FILE *out);
void blk_measure(const BlkTable *t, BlkAsl *a);
void blk_print_asl(const BlkTable *t, FILE *out);
int blk_find(const BlkTable *t, int key, FILE *out, int *cmp);
typedef struct {
    long long num;
    long long den;
} BlkFrac;

int blk_formula(int index_bin, int inner_bin, int blocks, int size, BlkFrac *out);

typedef struct {
    int index_bin;
    int inner_bin;
    int n;
    const int *sizes;
    int ns;
} BestAsk;

int blk_best(const BestAsk *ask, FILE *out);

#endif
