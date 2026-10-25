#ifndef MATCH_H
#define MATCH_H

#include <stdio.h>

/*
 * BF 固定按王道从 1 起：失配时 i = i - j + 2，j 回到 1，主串指针会退。
 * KMP 的 i 只增不减。base 决定打印出来的 i、j 和失败位置：
 * 成功时，从 1 起的位置 = 从 0 起的位置 + 1；
 * 失败时，从 1 起返回 0，从 0 起返回 -1，同样差 1。
 * use_val 为 0 用 next，非 0 用 nextval。哨兵那一步不比较字符，不计次数。
 */

typedef struct {
    int pos;
    int compares;
    int missed;
    int miss_i;
    int miss_j;
    int next_i;
    int next_j;
} MatchInfo;

int match_bf(const char *text, const char *pat, FILE *out, MatchInfo *info);
int match_kmp(const char *text, const char *pat, int base, int use_val, FILE *out, MatchInfo *info);

#endif
