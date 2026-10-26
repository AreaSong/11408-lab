#ifndef BT_PRIV_H
#define BT_PRIV_H

#include "bt.h"

BtNode *bt_new(BtTree *t);
void bt_release(BtTree *t, BtNode *node);
void bt_free_sub(BtNode *node);
int bt_leaf(const BtNode *node);
void bt_write_keys(FILE *out, const int *key, int n);

#endif
