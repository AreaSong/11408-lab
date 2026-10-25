#ifndef TREE_H
#define TREE_H

#include <stdio.h>

/* 二叉链表。王道的结点是 BiTNode，左右孩子是指针。
 * 第 03 关的顺序栈和第 04 关的循环队列元素都是 int，64 位指针放不进去，
 * 所以孩子改成编号，0 表示空。编号从 1 起，和“下标 0 不用”的习惯一致。
 * 这仍是二叉链表：孩子存在结点自己的 left、right 里。
 * 顺序存储不存这两个字段，孩子靠 2i、2i+1 算，见 seq.c。 */
#define BT_MAX 64

typedef struct {
    int data;
    int left;
    int right;
} BtNode;

typedef struct {
    BtNode node[BT_MAX];
    int n;
    int root;
} BiTree;

typedef struct {
    int n;
    int n0;
    int n1;
    int n2;
    int nulls;
    int height;
    int nonleaf;
    int no_left;
    int no_right;
    int nonleaf_deg1;
    int nonleaf_deg2;
    int all_deg1;
    int all_deg2;
    int pre_eq_in;
} TreeInfo;

void tree_init(BiTree *t);

/* 失败返回 0。成功的编号从 1 起，不会和空结点混淆。 */
int tree_alloc(BiTree *t, int data);

/* 先序串，'#' 是空孩子。0 成功，-1 串提前结束，-2 还有多余字符，-3 结点太多。 */
int tree_from_pre(BiTree *t, const char *s);

int tree_height(const BiTree *t, int id);

int tree_pre(const BiTree *t, int id, char *buf, int cap);
int tree_in(const BiTree *t, int id, char *buf, int cap);
int tree_post(const BiTree *t, int id, char *buf, int cap);

/* 从根统计。字符串放进 pre/inord/post，长度不够返回 -1。 */
int tree_info(const BiTree *t, TreeInfo *info, char *pre, char *inord, char *post, int cap);

/* 递归遍历的进入、访问、离开。访问点分别在左右子树之前、之间、之后。 */
void tree_trace(const BiTree *t, FILE *out);

#endif
