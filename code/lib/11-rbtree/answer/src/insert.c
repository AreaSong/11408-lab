#include "insert.h"

#include <stdlib.h>

static void trace_uncle(FILE *out, const RbTree *t, const RbNode *uncle, const char *kind) {
    if (!out) {
        return;
    }
    if (uncle == t->nil) {
        fprintf(out, "uncle # B kind=%s\n", kind);
        return;
    }
    fprintf(out, "uncle %d %c kind=%s\n", uncle->key, uncle->red ? 'R' : 'B', kind);
}

static void trace_recolor_uncle(FILE *out, int parent, int uncle, int grand) {
    if (out) {
        fprintf(out, "recolor parent %d B uncle %d B grand %d R\n", parent, uncle, grand);
    }
}

static void trace_recolor_pg(FILE *out, int parent, int grand) {
    if (out) {
        fprintf(out, "recolor parent %d B grand %d R\n", parent, grand);
    }
}

static void trace_rotate(FILE *out, int key, const char *dir) {
    if (out) {
        fprintf(out, "rotate %d %s\n", key, dir);
    }
}

static void add_spin(int *spins) {
    if (spins) {
        (*spins)++;
    }
}

/* 父结点是祖父的左孩子。叔红只变色；叔黑时 LR 先拉直，再按 LL 右旋。 */
static void fix_left_line(RbTree *t, RbNode **zp, FILE *out, int *spins) {
    RbNode *z = *zp;
    RbNode *uncle = z->parent->parent->right;
    int parent_key;
    int grand_key;
    if (uncle->red) {
        parent_key = z->parent->key;
        grand_key = z->parent->parent->key;
        trace_uncle(out, t, uncle, "recolor");
        trace_recolor_uncle(out, parent_key, uncle->key, grand_key);
        z->parent->red = 0;
        uncle->red = 0;
        z->parent->parent->red = 1;
        *zp = z->parent->parent;
        return;
    }
    if (z == z->parent->right) {
        trace_uncle(out, t, uncle, "LR");
        trace_rotate(out, z->parent->key, "left");
        z = z->parent;
        rb_rotate_left(t, z);
        add_spin(spins);
        *zp = z;
    } else {
        trace_uncle(out, t, uncle, "LL");
    }
    parent_key = z->parent->key;
    grand_key = z->parent->parent->key;
    trace_recolor_pg(out, parent_key, grand_key);
    trace_rotate(out, grand_key, "right");
    z->parent->red = 0;
    z->parent->parent->red = 1;
    rb_rotate_right(t, z->parent->parent);
    add_spin(spins);
}

static void fix_right_line(RbTree *t, RbNode **zp, FILE *out, int *spins) {
    RbNode *z = *zp;
    RbNode *uncle = z->parent->parent->left;
    int parent_key;
    int grand_key;
    if (uncle->red) {
        parent_key = z->parent->key;
        grand_key = z->parent->parent->key;
        trace_uncle(out, t, uncle, "recolor");
        trace_recolor_uncle(out, parent_key, uncle->key, grand_key);
        z->parent->red = 0;
        uncle->red = 0;
        z->parent->parent->red = 1;
        *zp = z->parent->parent;
        return;
    }
    if (z == z->parent->left) {
        trace_uncle(out, t, uncle, "RL");
        trace_rotate(out, z->parent->key, "right");
        z = z->parent;
        rb_rotate_right(t, z);
        add_spin(spins);
        *zp = z;
    } else {
        trace_uncle(out, t, uncle, "RR");
    }
    parent_key = z->parent->key;
    grand_key = z->parent->parent->key;
    trace_recolor_pg(out, parent_key, grand_key);
    trace_rotate(out, grand_key, "left");
    z->parent->red = 0;
    z->parent->parent->red = 1;
    rb_rotate_left(t, z->parent->parent);
    add_spin(spins);
}

static void insert_fix(RbTree *t, RbNode *z, FILE *out, int *spins) {
    while (z->parent->red) {
        if (z->parent == z->parent->parent->left) {
            fix_left_line(t, &z, out, spins);
        } else {
            fix_right_line(t, &z, out, spins);
        }
    }
    /* 变色可能把根涂红。根改回黑色会让每条路径同时多一个黑，性质⑤还在。 */
    if (t->root->red) {
        if (out) {
            fprintf(out, "root %d B\n", t->root->key);
        }
        t->root->red = 0;
    }
}

int rb_insert(RbTree *t, int key, FILE *out, int *spins) {
    RbNode *z = rb_new(t, key, 1);
    RbNode *parent = t->nil;
    RbNode *curr = t->root;
    if (spins) {
        *spins = 0;
    }
    while (curr != t->nil) {
        parent = curr;
        if (key == curr->key) {
            free(z);
            return 1;
        }
        curr = key < curr->key ? curr->left : curr->right;
    }
    z->parent = parent;
    if (parent == t->nil) {
        t->root = z;
    } else if (key < parent->key) {
        parent->left = z;
    } else {
        parent->right = z;
    }
    insert_fix(t, z, out, spins);
    return 0;
}
