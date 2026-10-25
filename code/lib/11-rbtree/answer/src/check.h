#ifndef CHECK_H
#define CHECK_H

#include "tree.h"

#include <stdio.h>

/*
 * bh 是根的黑高：从根往下走到 NIL，不含根、含 NIL，黑结点数。
 * 空树黑高是 0。左右贡献不相等时 bh_ok 为 0，bh 为 -1。
 * root_state：0 空树，1 黑根，2 红根。
 * long_path / short_path 是根到 NIL 的边数，等于这条路上的内部结点数。
 */
typedef struct RbStats {
    int nodes;
    int reds;
    int height;
    int bh;
    int bh_ok;
    int bst;
    int color_ok;
    int root_state;
    int root_black;
    int nil_black;
    int redred;
    int has_pair;
    int pair_parent;
    int pair_child;
    int long_path;
    int short_path;
    int rb;
} RbStats;

void rb_stats(const RbTree *t, RbStats *s);
void rb_dump(const RbTree *t, FILE *out);

#endif
