#ifndef NODE_H
#define NODE_H

#include <stdio.h>

/*
 * 二叉链表。height 是结点高度：空树算 0，叶子算 1。
 * 平衡因子不另存，用左右子树高度相减，避免旋转后忘记改。
 */
typedef struct Node {
    int key;
    struct Node *left;
    struct Node *right;
    int height;
} Node;

Node *node_new(int key);
void node_free(Node *t);
Node *node_clone(const Node *t);

int node_height(const Node *t);
int node_bf(const Node *t);
void node_pull(Node *t);

int node_count(const Node *t);
int node_equal(const Node *a, const Node *b);
int node_bst_ok(const Node *t);
int node_height_ok(const Node *t);
const Node *node_find(const Node *t, int key);

/* with_bf 非 0 时在关键字后面写 [平衡因子]。空结点写成 #，叶子不写括号。 */
void node_fprint(const Node *t, FILE *out, int with_bf);
int node_shape(const Node *t, char *buf, int cap, int with_bf);
void node_inorder(const Node *t, FILE *out);
void node_levels(const Node *t, FILE *out);

#endif
