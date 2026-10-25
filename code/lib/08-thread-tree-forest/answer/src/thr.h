#ifndef THR_H
#define THR_H

#include <stdio.h>

/* 线索二叉树。王道的结点是指针加 ltag/rtag，这里改成编号：
 * 0 表示空。ltag/rtag 为 0 时指针是孩子，为 1 时是线索。
 * 中序线索化按王道带头结点：头结点编号固定放在最后一格，打印成 ^。
 * 后序线索只按定义把空孩子改成前驱或后继，不带头结点。 */
#define THR_MAX 64
#define THR_HEAD 63

enum {
    THR_OK = 0,
    THR_FORMAT = -1,
    THR_EXTRA = -2,
    THR_SIZE = -3,
    THR_EMPTY = -4,
    THR_DUP = -5,
    THR_MISS = -6,
    THR_MISMATCH = -7,
    THR_RANGE = -8,
    THR_SHORT = -9
};

typedef struct {
    char data;
    int left;
    int right;
    int ltag;
    int rtag;
    int parent;
} ThrNode;

typedef struct {
    ThrNode node[THR_MAX];
    int n;
    int root;
    int head;
} ThrTree;

void thr_init(ThrTree *t);

/* 先序串，'#' 是空孩子。'#' 单独出现表示空树。 */
int thr_from_pre(ThrTree *t, const char *s);

int thr_find(const ThrTree *t, char data);

/* 中序线索化并打印。ask 为 0 则不单问结点。trace 写进 out。 */
int thr_show_in(FILE *out, const char *pre, int ask);

/* 后序线索。ask 必须是树里的结点。 */
int thr_show_post(FILE *out, const char *pre, int ask);

/* 中序里相邻的每一对，标出双亲、右孩子、左右兄弟、祖父。 */
int thr_show_pairs(FILE *out, const char *pre);

/* 下面几个给单元测试用，调用前先 thr_from_pre。 */
int thr_thread_in(ThrTree *t);
int thr_pred(const ThrTree *t, int id);
int thr_succ(const ThrTree *t, int id);
int thr_inorder(const ThrTree *t, char *buf, int cap);
int thr_sibling_adj(const ThrTree *t);
int thr_two_child_ok(const ThrTree *t);

/* 后序线索的右线索。out_ch 为 0 表示空。树必须已 thr_from_pre 且尚未线索化。 */
int thr_post_right(ThrTree *t, char ask, char *out_ch);

#endif
