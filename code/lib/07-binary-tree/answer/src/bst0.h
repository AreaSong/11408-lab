#ifndef BST0_H
#define BST0_H

/* 2022-41：顺序存储从下标 0 起，空位是 -1，左孩子 2i+1，右孩子 2i+2。 */
#define BST0_MAX 32

/* 是二叉搜索树返回 1，否则返回 0。没有真实结点也返回 0。 */
int bst0_is(const int *a, int n);

/* 中序写入 seq，*sn 是结点个数。参数不合法返回 -1。 */
int bst0_inorder(const int *a, int n, int *seq, int *sn);

#endif
