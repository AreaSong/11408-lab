#include "bp.h"

#include "limit.h"

#include <stdlib.h>

BpTree *bp_create(int order) {
    BpTree *tree;
    if (!limit_order(order)) {
        return NULL;
    }
    tree = calloc(1, sizeof *tree);
    if (!tree) {
        exit(1);
    }
    tree->order = order;
    return tree;
}

static BpNode *bp_new(BpTree *tree, int leaf) {
    BpNode *node = calloc(1, sizeof *node);
    if (!node) {
        exit(1);
    }
    node->leaf = leaf;
    tree->node_count++;
    return node;
}

static void free_sub(BpNode *node) {
    if (!node) {
        return;
    }
    if (!node->leaf) {
        for (int i = 0; i < node->n; i++) {
            free_sub(node->child[i]);
        }
    }
    free(node);
}

void bp_free(BpTree *tree) {
    if (!tree) {
        return;
    }
    free_sub(tree->root);
    free(tree);
}

int bp_height(const BpTree *tree) {
    int height = 0;
    const BpNode *node = tree->root;
    while (node) {
        height++;
        if (node->leaf) {
            break;
        }
        node = node->child[0];
    }
    return height;
}

static void write_keys(FILE *out, const int *key, int n) {
    fputc('[', out);
    for (int i = 0; i < n; i++) {
        if (i) {
            fputc(' ', out);
        }
        fprintf(out, "%d", key[i]);
    }
    fputc(']', out);
}

static BpNode *leftmost(const BpTree *tree) {
    BpNode *node = tree->root;
    while (node && !node->leaf) {
        node = node->child[0];
    }
    return node;
}

static void write_node(const BpNode *node, FILE *out) {
    fputc('(', out);
    for (int i = 0; i < node->n; i++) {
        if (i) {
            fputc(' ', out);
        }
        fprintf(out, "%d", node->key[i]);
    }
    if (!node->leaf) {
        for (int i = 0; i < node->n; i++) {
            fputc(' ', out);
            write_node(node->child[i], out);
        }
    }
    fputc(')', out);
}

static int chain_ok(const BpTree *tree) {
    BpNode *leaf = leftmost(tree);
    int seen = 0;
    int guard = 0;
    int last = 0;
    int has_last = 0;
    int leaves = 0;
    if (!tree->root) {
        return 1;
    }
    while (leaf) {
        if (!leaf->leaf || ++guard > tree->node_count) {
            return 0;
        }
        for (int i = 0; i < leaf->n; i++) {
            if (has_last && leaf->key[i] <= last) {
                return 0;
            }
            last = leaf->key[i];
            has_last = 1;
            seen++;
        }
        leaves++;
        leaf = leaf->next;
    }
    return seen == tree->key_count && leaves > 0;
}

void bp_report(const BpTree *tree, FILE *out) {
    BpNode *leaf = leftmost(tree);
    int first = 1;
    fprintf(out, "tree ");
    if (!tree->root) {
        fprintf(out, "empty\n");
    } else {
        write_node(tree->root, out);
        fputc('\n', out);
    }
    fprintf(out, "height=%d keys=%d nodes=%d\n", bp_height(tree), tree->key_count,
            tree->node_count);
    fprintf(out, "root");
    if (!tree->root) {
        fprintf(out, " empty\n");
    } else {
        for (int i = 0; i < tree->root->n; i++) {
            fprintf(out, " %d", tree->root->key[i]);
        }
        fputc('\n', out);
    }
    fprintf(out, "chain=%s\n", chain_ok(tree) && tree->root ? "yes" : "no");
    fprintf(out, "leaf");
    while (leaf) {
        if (!first) {
            fputs(" | ", out);
        } else {
            fputc(' ', out);
        }
        first = 0;
        for (int i = 0; i < leaf->n; i++) {
            if (i) {
                fputc(' ', out);
            }
            fprintf(out, "%d", leaf->key[i]);
        }
        leaf = leaf->next;
    }
    fputc('\n', out);
}

int bp_leaves(const BpTree *tree, int *out, int cap) {
    BpNode *leaf = leftmost(tree);
    int n = 0;
    while (leaf) {
        for (int i = 0; i < leaf->n; i++) {
            if (n >= cap) {
                return -1;
            }
            out[n++] = leaf->key[i];
        }
        leaf = leaf->next;
    }
    return n;
}

static int child_slot(const BpNode *parent, const BpNode *child) {
    for (int i = 0; i < parent->n; i++) {
        if (parent->child[i] == child) {
            return i;
        }
    }
    return -1;
}

static void bubble_max(BpNode *node, FILE *tr) {
    while (node->parent) {
        BpNode *parent = node->parent;
        int slot = child_slot(parent, node);
        int max_key = node->key[node->n - 1];
        if (slot < 0 || parent->key[slot] == max_key) {
            return;
        }
        if (tr) {
            fprintf(tr, "copy %d -> %d\n", parent->key[slot], max_key);
        }
        parent->key[slot] = max_key;
        if (slot != parent->n - 1) {
            return;
        }
        node = parent;
    }
}

static void insert_after(BpNode *parent, int slot, int key, BpNode *right) {
    for (int j = parent->n; j > slot + 1; j--) {
        parent->key[j] = parent->key[j - 1];
        parent->child[j] = parent->child[j - 1];
    }
    parent->key[slot + 1] = key;
    parent->child[slot + 1] = right;
    right->parent = parent;
    parent->n++;
}

/*
 * 叶结点分裂后关键字仍留在叶里，父结点只复写左右两半的最大关键字。
 * 内部结点的关键字本来就是副本，分裂时按同样的份数切开。
 */
static void trace_split(FILE *tr, const BpTree *tree, const BpNode *left, const BpNode *right,
                        int left_max, int right_max) {
    if (!tr) {
        return;
    }
    fprintf(tr, "split %s left=", left->leaf ? "leaf" : "index");
    write_keys(tr, left->key, left->n);
    fprintf(tr, " right=");
    write_keys(tr, right->key, right->n);
    fprintf(tr, " copy=%d %d height=%d\n", left_max, right_max, bp_height(tree));
}

static BpNode *split_node(BpTree *tree, BpNode *node, FILE *tr) {
    int keep = (tree->order + 1) / 2;
    BpNode *right = bp_new(tree, node->leaf);
    int left_max;
    int right_max;
    right->n = node->n - keep;
    for (int i = 0; i < right->n; i++) {
        right->key[i] = node->key[keep + i];
        if (!node->leaf) {
            right->child[i] = node->child[keep + i];
            right->child[i]->parent = right;
            node->child[keep + i] = NULL;
        }
    }
    node->n = keep;
    if (node->leaf) {
        right->next = node->next;
        node->next = right;
    }
    left_max = node->key[node->n - 1];
    right_max = right->key[right->n - 1];
    if (!node->parent) {
        BpNode *root = bp_new(tree, 0);
        root->n = 2;
        root->key[0] = left_max;
        root->key[1] = right_max;
        root->child[0] = node;
        root->child[1] = right;
        node->parent = root;
        right->parent = root;
        tree->root = root;
    } else {
        int slot = child_slot(node->parent, node);
        node->parent->key[slot] = left_max;
        insert_after(node->parent, slot, right_max, right);
    }
    trace_split(tr, tree, node, right, left_max, right_max);
    /* 父结点自己没满时，最大关键字的变化要继续抄到更上层。父结点也满了就等下一轮分裂来改。 */
    if (node->parent && node->parent->n > tree->order) {
        return node->parent;
    }
    if (node->parent && node->parent->parent) {
        bubble_max(node->parent, tr);
    }
    return NULL;
}

static BpNode *find_leaf(BpNode *node, int key, int *found) {
    *found = 0;
    while (node && !node->leaf) {
        int i = 0;
        while (i < node->n && key > node->key[i]) {
            i++;
        }
        if (i == node->n) {
            i = node->n - 1;
        }
        node = node->child[i];
    }
    if (!node) {
        return NULL;
    }
    for (int i = 0; i < node->n; i++) {
        if (node->key[i] == key) {
            *found = 1;
        }
    }
    return node;
}

static void put_leaf(FILE *tr, const BpNode *node) {
    if (!tr) {
        return;
    }
    fprintf(tr, "put ");
    write_keys(tr, node->key, node->n);
    fputc('\n', tr);
}

static void leaf_insert(BpNode *leaf, int key) {
    int index = 0;
    while (index < leaf->n && leaf->key[index] < key) {
        index++;
    }
    for (int j = leaf->n; j > index; j--) {
        leaf->key[j] = leaf->key[j - 1];
    }
    leaf->key[index] = key;
    leaf->n++;
}

int bp_insert(BpTree *tree, int key, FILE *tr) {
    int found = 0;
    BpNode *leaf;
    if (tree->key_count >= BP_MAX_N) {
        return BP_RANGE;
    }
    if (!tree->root) {
        tree->root = bp_new(tree, 1);
        tree->root->key[0] = key;
        tree->root->n = 1;
        tree->key_count = 1;
        put_leaf(tr, tree->root);
        return BP_OK;
    }
    leaf = find_leaf(tree->root, key, &found);
    if (found) {
        return BP_DUP;
    }
    leaf_insert(leaf, key);
    tree->key_count++;
    if (leaf->n <= tree->order) {
        put_leaf(tr, leaf);
        if (leaf->key[leaf->n - 1] == key) {
            bubble_max(leaf, tr);
        }
        return BP_OK;
    }
    while (leaf && leaf->n > tree->order) {
        leaf = split_node(tree, leaf, tr);
    }
    return BP_OK;
}

static void find_walk(const BpNode *node, int key, FILE *out) {
    while (node) {
        int i = 0;
        fprintf(out, "node ");
        write_keys(out, node->key, node->n);
        fputc('\n', out);
        while (i < node->n && node->key[i] < key) {
            fprintf(out, "%d >\n", node->key[i]);
            i++;
        }
        if (i < node->n && node->key[i] == key) {
            fprintf(out, "%d =%s\n", key, node->leaf ? "" : " copy");
            if (node->leaf) {
                fprintf(out, "hit leaf=yes\n");
                return;
            }
        } else if (i < node->n) {
            fprintf(out, "%d <\n", node->key[i]);
        }
        if (node->leaf) {
            fprintf(out, "miss leaf=yes\n");
            return;
        }
        if (i == node->n) {
            i = node->n - 1;
        }
        node = node->child[i];
    }
}

int bp_find(const BpTree *tree, int key, FILE *tr) {
    int found = 0;
    if (!tree->root) {
        return BP_MISS;
    }
    if (tr) {
        find_walk(tree->root, key, tr);
    }
    find_leaf(tree->root, key, &found);
    return found ? BP_OK : BP_MISS;
}

static int keys_fit(const BpNode *node, int has_lo, int lo) {
    for (int i = 0; i < node->n; i++) {
        if ((i > 0 && node->key[i] <= node->key[i - 1]) || (has_lo && node->key[i] <= lo)) {
            return 0;
        }
    }
    return 1;
}

static int check_node(const BpTree *tree, const BpNode *node, int is_root, int has_lo, int lo,
                      int depth, int *leaf_depth, int *nodes) {
    int lo_n = is_root ? (node->leaf ? 1 : 2) : limit_ceil_half(tree->order);
    if (!node || node->n < lo_n || node->n > tree->order || !keys_fit(node, has_lo, lo)) {
        return 0;
    }
    (*nodes)++;
    if (node->leaf) {
        if (*leaf_depth < 0) {
            *leaf_depth = depth;
        }
        return *leaf_depth == depth;
    }
    for (int i = 0; i < node->n; i++) {
        const BpNode *child = node->child[i];
        int child_lo = i > 0;
        if (!child || child->parent != node || child->key[child->n - 1] != node->key[i]) {
            return 0;
        }
        if (!check_node(tree, child, 0, child_lo, child_lo ? node->key[i - 1] : 0, depth + 1,
                        leaf_depth, nodes)) {
            return 0;
        }
    }
    return 1;
}

static int count_leaves(const BpNode *node) {
    int n = 0;
    if (!node) {
        return 0;
    }
    if (node->leaf) {
        return 1;
    }
    for (int i = 0; i < node->n; i++) {
        n += count_leaves(node->child[i]);
    }
    return n;
}

int bp_valid(const BpTree *tree) {
    int leaf_depth = -1;
    int nodes = 0;
    int on_chain = 0;
    BpNode *leaf;
    if (!tree->root) {
        return tree->key_count == 0 && tree->node_count == 0;
    }
    if (tree->root->parent) {
        return 0;
    }
    if (!check_node(tree, tree->root, 1, 0, 0, 1, &leaf_depth, &nodes)) {
        return 0;
    }
    if (nodes != tree->node_count || !chain_ok(tree)) {
        return 0;
    }
    leaf = leftmost(tree);
    while (leaf) {
        on_chain++;
        leaf = leaf->next;
    }
    return on_chain == count_leaves(tree->root);
}
