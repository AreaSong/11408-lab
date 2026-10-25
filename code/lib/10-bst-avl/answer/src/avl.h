#ifndef AVL_H
#define AVL_H

#include "node.h"

#include <stdio.h>

/*
 * desc 为 0：左小右大，中序递增。
 * desc 为 1：左大右小，中序递减。旋转只看高度，不看关键字方向。
 * out 非空时，失衡才打印最小不平衡子树的旋转过程。
 */
Node *avl_insert(Node *root, int key, int desc, FILE *out, int *dup);
Node *avl_delete(Node *root, int key, FILE *out, int *found);

/* 空树高 0 时 N(0)=0，N(1)=1，N(h)=N(h-1)+N(h-2)+1。 */
int avl_min_count(int h);

/* 真的把最少结点的那棵树造出来：左子树高 h-1，右子树高 h-2，中序关键字是 1..n。 */
Node *avl_build_min(int h);
void avl_min_report(FILE *out, int h);

#endif
