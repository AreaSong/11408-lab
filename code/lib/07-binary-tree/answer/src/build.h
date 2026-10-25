#ifndef BUILD_H
#define BUILD_H

#include "tree.h"

#include <stdio.h>

/* 先序+中序、后序+中序都能唯一确定。返回根编号，空子树返回 0。
 * -2 有重复或非法字符，-3 对不上，-4 结点太多。trace 打印每次如何切开。 */
int build_pre_in(BiTree *t, const char *pre, const char *inord, FILE *trace);
int build_post_in(BiTree *t, const char *post, const char *inord, FILE *trace);

#define PREPOST_MAX 32
#define FORM_IN 16
#define FORM_SHAPE 96

typedef struct {
    char inord[FORM_IN];
    char shape[FORM_SHAPE];
} BtForm;

/* 先序+后序。正数是满足这两条序列的树的棵数，0 是一条也没有。
 * -1 空串或长度不同，-2 字符非法或重复，-3 字符集合不同，
 * -4 长于 6，-5 棵数超过 cap。n 个结点的单链恰好 2^(n-1) 棵，6 个结点是 32。 */
int prepost_forms(const char *pre, const char *post, BtForm *out, int cap);

#endif
