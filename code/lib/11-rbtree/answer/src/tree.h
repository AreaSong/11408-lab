#ifndef TREE_H
#define TREE_H

#include <stdio.h>

#define RB_KEY_MAX 64
#define RB_KEY_LO (-1000000)
#define RB_KEY_HI 1000000

/*
 * 红黑树的叶结点是外部空结点 NIL，不是「没有孩子的内部结点」。
 * 全树共用一个黑色哨兵：画树时省略它，数黑高时要把它算进去。
 * red 为 1 表示红，0 表示黑。新的内部结点先染红，根最后再涂黑。
 */
typedef struct RbNode {
    int key;
    int red;
    struct RbNode *left;
    struct RbNode *right;
    struct RbNode *parent;
} RbNode;

typedef struct RbTree {
    RbNode *root;
    RbNode *nil;
} RbTree;

RbTree *rb_create(void);
void rb_free(RbTree *t);
void rb_free_sub(RbTree *t, RbNode *n);

RbNode *rb_new(RbTree *t, int key, int red);
RbNode *rb_find(const RbTree *t, int key);
RbNode *rb_min(const RbTree *t, RbNode *n);

void rb_rotate_left(RbTree *t, RbNode *x);
void rb_rotate_right(RbTree *t, RbNode *x);

void rb_fprint(const RbTree *t, const RbNode *n, FILE *out);
void rb_print_inorder(const RbTree *t, FILE *out);

#endif
