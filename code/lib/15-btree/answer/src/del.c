#include "bt_priv.h"

#include "limit.h"

static int slot_of(const BtNode *parent, const BtNode *child) {
    for (int i = 0; i <= parent->n; i++) {
        if (parent->child[i] == child) {
            return i;
        }
    }
    return -1;
}

static void drop_parent(BtNode *parent, int key_i, int child_i) {
    for (int j = key_i; j < parent->n - 1; j++) {
        parent->key[j] = parent->key[j + 1];
    }
    for (int j = child_i; j < parent->n; j++) {
        parent->child[j] = parent->child[j + 1];
    }
    parent->child[parent->n] = NULL;
    parent->n--;
}

static void take_key(BtNode *node, int index) {
    for (int j = index; j < node->n - 1; j++) {
        node->key[j] = node->key[j + 1];
    }
    node->n--;
}

static BtNode *edge_leaf(BtNode *node, int go_right) {
    while (!bt_leaf(node)) {
        node = go_right ? node->child[node->n] : node->child[0];
    }
    return node;
}

static int locate(BtNode *node, int key, BtNode **out, int *index) {
    while (node) {
        int i = 0;
        while (i < node->n && key > node->key[i]) {
            i++;
        }
        if (i < node->n && node->key[i] == key) {
            *out = node;
            *index = i;
            return 1;
        }
        if (bt_leaf(node)) {
            return 0;
        }
        node = node->child[i];
    }
    return 0;
}

/* 向左借：左兄弟的最大关键字上移，父结点的分隔关键字下来。 */
static void borrow_left(BtNode *node, BtNode *left, BtNode *parent, int slot, FILE *tr) {
    int down = parent->key[slot - 1];
    int up = left->key[left->n - 1];
    BtNode *moved = left->child[left->n];
    for (int j = node->n; j > 0; j--) {
        node->key[j] = node->key[j - 1];
        node->child[j + 1] = node->child[j];
    }
    node->child[1] = node->child[0];
    node->key[0] = down;
    node->child[0] = moved;
    if (moved) {
        moved->parent = node;
    }
    node->n++;
    parent->key[slot - 1] = up;
    left->child[left->n] = NULL;
    left->n--;
    if (tr) {
        fprintf(tr, "borrow left up=%d down=%d\n", up, down);
    }
}

static void borrow_right(BtNode *node, BtNode *right, BtNode *parent, int slot, FILE *tr) {
    int down = parent->key[slot];
    int up = right->key[0];
    BtNode *moved = right->child[0];
    node->key[node->n] = down;
    node->child[node->n + 1] = moved;
    if (moved) {
        moved->parent = node;
    }
    node->n++;
    parent->key[slot] = up;
    for (int j = 0; j < right->n - 1; j++) {
        right->key[j] = right->key[j + 1];
    }
    for (int j = 0; j < right->n; j++) {
        right->child[j] = right->child[j + 1];
    }
    right->child[right->n] = NULL;
    right->n--;
    if (tr) {
        fprintf(tr, "borrow right up=%d down=%d\n", up, down);
    }
}

/* 合并把父结点的分隔关键字拉下来，和兄弟拼成一个结点。 */
static void absorb(BtNode *dst, int sep, BtNode *src) {
    int base = dst->n;
    dst->key[base] = sep;
    dst->child[base + 1] = src->child[0];
    if (dst->child[base + 1]) {
        dst->child[base + 1]->parent = dst;
    }
    for (int j = 0; j < src->n; j++) {
        dst->key[base + 1 + j] = src->key[j];
        dst->child[base + 2 + j] = src->child[j + 1];
        if (dst->child[base + 2 + j]) {
            dst->child[base + 2 + j]->parent = dst;
        }
    }
    dst->n = base + 1 + src->n;
}

static void merge_left(BtTree *tree, BtNode *node, BtNode *left, BtNode *parent, int slot, FILE *tr) {
    int sep = parent->key[slot - 1];
    absorb(left, sep, node);
    drop_parent(parent, slot - 1, slot);
    bt_release(tree, node);
    if (tr) {
        fprintf(tr, "merge left down=%d\n", sep);
    }
}

static void merge_right(BtTree *tree, BtNode *node, BtNode *right, BtNode *parent, int slot, FILE *tr) {
    int sep = parent->key[slot];
    absorb(node, sep, right);
    drop_parent(parent, slot, slot + 1);
    bt_release(tree, right);
    if (tr) {
        fprintf(tr, "merge right down=%d\n", sep);
    }
}

static void shrink_root(BtTree *tree, BtNode *node, FILE *tr) {
    BtNode *child;
    if (node->n > 0) {
        return;
    }
    child = node->child[0];
    bt_release(tree, node);
    tree->root = child;
    if (child) {
        child->parent = NULL;
        if (tr) {
            fprintf(tr, "shrink\n");
        }
    } else if (tr) {
        fprintf(tr, "empty\n");
    }
}

static void fix(BtTree *tree, BtNode *node, BtPolicy policy, FILE *tr) {
    int min_keys;
    int slot;
    BtNode *parent;
    BtNode *left;
    BtNode *right;
    if (node == tree->root) {
        shrink_root(tree, node, tr);
        return;
    }
    min_keys = limit_min_keys(tree->order, 0);
    if (node->n >= min_keys) {
        return;
    }
    parent = node->parent;
    slot = slot_of(parent, node);
    left = slot > 0 ? parent->child[slot - 1] : NULL;
    right = slot >= 0 && slot < parent->n ? parent->child[slot + 1] : NULL;
    if (left && left->n > min_keys) {
        borrow_left(node, left, parent, slot, tr);
        return;
    }
    if (right && right->n > min_keys) {
        borrow_right(node, right, parent, slot, tr);
        return;
    }
    if (left && !(right && policy.merge_right)) {
        merge_left(tree, node, left, parent, slot, tr);
    } else {
        merge_right(tree, node, right, parent, slot, tr);
    }
    fix(tree, parent, policy, tr);
}

static void replace_with_terminal(BtNode **node, int *index, BtPolicy policy, FILE *tr) {
    BtNode *term;
    int leaf_i;
    if (policy.use_succ) {
        term = edge_leaf((*node)->child[*index + 1], 0);
        leaf_i = 0;
        if (tr) {
            fprintf(tr, "succ %d\n", term->key[leaf_i]);
        }
    } else {
        term = edge_leaf((*node)->child[*index], 1);
        leaf_i = term->n - 1;
        if (tr) {
            fprintf(tr, "pred %d\n", term->key[leaf_i]);
        }
    }
    (*node)->key[*index] = term->key[leaf_i];
    *node = term;
    *index = leaf_i;
}

int bt_delete(BtTree *tree, int key, BtPolicy policy, FILE *tr) {
    BtNode *node = NULL;
    int index = 0;
    if (!tree->root || !locate(tree->root, key, &node, &index)) {
        return BT_MISS;
    }
    if (!bt_leaf(node)) {
        replace_with_terminal(&node, &index, policy, tr);
    }
    take_key(node, index);
    tree->key_count--;
    fix(tree, node, policy, tr);
    return BT_OK;
}
