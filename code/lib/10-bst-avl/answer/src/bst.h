#ifndef BST_H
#define BST_H

#include "node.h"

#include <stdio.h>

typedef struct {
    long long succ_num;
    long long succ_den;
    long long fail_num;
    long long fail_den;
} Asl;

/* *dup 由函数置位。重复关键字不插入。 */
Node *bst_insert(Node *root, int key, int *dup);

/*
 * 删除。*found 为 0 表示没有这个关键字。
 * out 非空时打印 case leaf / left / right / two，双孩子再打印 pred。
 * 双孩子用中序前驱（左子树最右结点）顶上，这是王道的写法。
 */
Node *bst_delete(Node *root, int key, FILE *out, int *found);

/* 沿查找路径每个经过的结点打一行 cmp。命中返回 1。 */
int bst_search(const Node *root, int key, FILE *out);

void bst_asl(const Node *root, Asl *asl);
void bst_asl_weight(const Node *root, const int *keys, const int *weight, int n, long long *num,
                    long long *den);

#endif
