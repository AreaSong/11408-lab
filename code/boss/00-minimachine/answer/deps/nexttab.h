#ifndef NEXTTAB_H
#define NEXTTAB_H

#include <stdio.h>

/*
 * 串与 KMP 的 next、nextval。
 *
 * 王道、严蔚敏：字符和 next 从 1 起，next[1] = 0。失配时 j 退到 next[j]，
 * 0 是哨兵，表示模式串第一个字符也对不上，主串和模式串一起前进一步。
 * next[j]（j>1）= 前 j-1 个字符的最长真前后缀长度 + 1。
 *
 * 另一套（2015、2019 题干里的位序，李春葆也这样写）：从 0 起，next[0] = -1。
 * next[j] 就是前 j 个字符的最长真前后缀长度，不再加 1。
 * 换算：next0[j] = next1[j+1] - 1。nextval 同样整体差 1。
 * 比较次数和右滑距离不受这套平移影响。
 */

#define STR_MAX 128

typedef struct {
    int base;
    int m;
    char s[STR_MAX + 1];      /* 字符始终放在 s[0..m-1]，与 base 无关 */
    int pm[STR_MAX + 1];      /* pm[k]：前 k 个字符的最长真前后缀长度，k = 1..m */
    int next[STR_MAX + 1];    /* 下标范围是 base .. base+m-1 */
    int nextval[STR_MAX + 1];
} Pattern;

/* 1 合法；0 空串；-1 超过 STR_MAX；-2 含非英文字母。 */
int kmp_check(const char *s);

/* 0 成功。会用「部分匹配值」和「按位修正」各验一遍递推结果。 */
int pattern_build(Pattern *p, int base, const char *s);

void pattern_print_next(const Pattern *p, FILE *out);
void pattern_print_nextval(const Pattern *p, FILE *out);

#endif
