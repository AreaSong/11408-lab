#ifndef BT_H
#define BT_H

#include <stdio.h>

#define BT_CAP 17
#define BT_KEY_LO (-1000000)
#define BT_KEY_HI 1000000
#define BT_MAX_N 256

enum { BT_OK = 0, BT_DUP = 1, BT_MISS = 2, BT_BAD = 3, BT_RANGE = 4, BT_FMT = 5 };

typedef struct BtNode {
    int n;
    int key[BT_CAP];
    struct BtNode *child[BT_CAP + 1];
    struct BtNode *parent;
} BtNode;

typedef struct BtTree {
    int order;
    BtNode *root;
    int key_count;
    int node_count;
} BtTree;

/* use_succ：非终端结点用直接后继；否则用直接前驱。
 * merge_right：左右兄弟都只剩下限时并到右兄弟；否则并到左兄弟。
 * 借的时候只要左兄弟有富余就先向左借，和合并方向无关。 */
typedef struct BtPolicy {
    int use_succ;
    int merge_right;
} BtPolicy;

BtTree *bt_create(int order);
void bt_free(BtTree *t);

int bt_insert(BtTree *t, int key, FILE *tr);
int bt_delete(BtTree *t, int key, BtPolicy policy, FILE *tr);
int bt_find(const BtTree *t, int key, FILE *tr);
int bt_load(BtTree *t, char **tok, int ntok);

int bt_valid(const BtTree *t);
int bt_height(const BtTree *t);
int bt_collect(const BtTree *t, int *out, int cap);
void bt_report(const BtTree *t, FILE *out);

#endif
