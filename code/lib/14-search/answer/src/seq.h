#ifndef SEQ_H
#define SEQ_H

#include "asl.h"
#include "tab.h"

#include <stdio.h>

/*
 * 顺序查找。下标从 1 起，和王道的 ST.elem[1..n] 一致。
 * plain：无序，从头扫，没有哨兵，失败比 n 次。
 * guard：无序，从尾扫，0 号是哨兵，失败比 n+1 次。
 * order：升序，从头扫，当前元素已经更大就停。成功 ASL 仍是 (n+1)/2。
 */
#define SEQ_PLAIN 0
#define SEQ_GUARD 1
#define SEQ_ORDER 2

typedef struct {
    int kind;
    int n;
    int weighted;
    int key[TABLE_MAX + 1];
    int weight[TABLE_MAX + 1];
} SeqTable;

int seq_kind(const char *name);
int seq_load(SeqTable *t, int kind, const int *keys, int n);
int seq_weight(SeqTable *t, const int *w, int n);
void seq_asl(const SeqTable *t, AslPair *a);
void seq_print_asl(const SeqTable *t, FILE *out);
void seq_report(const SeqTable *t, FILE *out);
/* 找到返回下标，失败返回 0。*cmp 是关键字比较次数。 */
int seq_find(const SeqTable *t, int key, FILE *out, int *cmp);

#endif
