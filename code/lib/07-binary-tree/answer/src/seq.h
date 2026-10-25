#ifndef SEQ_H
#define SEQ_H

#include "tree.h"

#include <stdio.h>

/* 顺序存储按下标算孩子，不在结点里存指针。
 * 下标从 1 起：左孩子 2i，右孩子 2i+1，双亲 i/2。这是王道的编号。
 * 2022-41 的数组从 0 起，左孩子 2i+1，右孩子 2i+2，空位写成 -1。
 * 两套差在起点。一般二叉树要按完全二叉树的编号补上空洞，否则下标公式对不上。 */
#define SEQ_MAX 64

typedef struct {
    int data[SEQ_MAX];
    int used[SEQ_MAX];
    int last;
    int n;
} SeqTree;

void seq_clear(SeqTree *s);

/* 0 成功。-1 太长或为空，-3 根是洞或洞下面还有结点，-4 词不是单个字符。 */
int seq_from_tokens(SeqTree *s, char **tok, int ntok);

/* 右斜链高度 7 会用到下标 127，数组只开到 63，这时返回 -1。 */
int seq_from_tree(const BiTree *t, SeqTree *s);

int seq_to_tree(const SeqTree *s, BiTree *t);

void seq_brief(const SeqTree *s, FILE *out);

void seq_relations(const SeqTree *s, FILE *out);

#endif
