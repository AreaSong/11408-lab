#include "bt_priv.h"

#include "limit.h"

#include <stdlib.h>

BtTree *bt_create(int order) {
    BtTree *tree;
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

int bt_leaf(const BtNode *node) {
    return node->child[0] == NULL;
}

BtNode *bt_new(BtTree *tree) {
    BtNode *node = calloc(1, sizeof *node);
    if (!node) {
        exit(1);
    }
    tree->node_count++;
    return node;
}

void bt_release(BtTree *tree, BtNode *node) {
    free(node);
    tree->node_count--;
}

void bt_write_keys(FILE *out, const int *key, int n) {
    fputc('[', out);
    for (int i = 0; i < n; i++) {
        if (i) {
            fputc(' ', out);
        }
        fprintf(out, "%d", key[i]);
    }
    fputc(']', out);
}

void bt_free_sub(BtNode *node) {
    if (!node) {
        return;
    }
    if (!bt_leaf(node)) {
        for (int i = 0; i <= node->n; i++) {
            bt_free_sub(node->child[i]);
        }
    }
    free(node);
}

void bt_free(BtTree *tree) {
    if (!tree) {
        return;
    }
    bt_free_sub(tree->root);
    free(tree);
}

int bt_height(const BtTree *tree) {
    int height = 0;
    const BtNode *node = tree->root;
    while (node) {
        height++;
        if (bt_leaf(node)) {
            break;
        }
        node = node->child[0];
    }
    return height;
}

static void collect_sub(const BtNode *node, int *out, int cap, int *n) {
    if (!node || *n < 0) {
        return;
    }
    if (bt_leaf(node)) {
        for (int i = 0; i < node->n; i++) {
            if (*n >= cap) {
                *n = -1;
                return;
            }
            out[(*n)++] = node->key[i];
        }
        return;
    }
    for (int i = 0; i < node->n; i++) {
        collect_sub(node->child[i], out, cap, n);
        if (*n < 0 || *n >= cap) {
            *n = -1;
            return;
        }
        out[(*n)++] = node->key[i];
    }
    collect_sub(node->child[node->n], out, cap, n);
}

int bt_collect(const BtTree *tree, int *out, int cap) {
    int n = 0;
    collect_sub(tree->root, out, cap, &n);
    return n;
}

static void write_node(const BtNode *node, FILE *out) {
    fputc('(', out);
    for (int i = 0; i < node->n; i++) {
        if (i) {
            fputc(' ', out);
        }
        fprintf(out, "%d", node->key[i]);
    }
    if (!bt_leaf(node)) {
        for (int i = 0; i <= node->n; i++) {
            fputc(' ', out);
            write_node(node->child[i], out);
        }
    }
    fputc(')', out);
}

static void write_terms(const BtNode *node, FILE *out, int *first) {
    if (bt_leaf(node)) {
        if (!*first) {
            fputs(" | ", out);
        }
        *first = 0;
        for (int i = 0; i < node->n; i++) {
            if (i) {
                fputc(' ', out);
            }
            fprintf(out, "%d", node->key[i]);
        }
        return;
    }
    for (int i = 0; i <= node->n; i++) {
        write_terms(node->child[i], out, first);
    }
}

void bt_report(const BtTree *tree, FILE *out) {
    int first = 1;
    fprintf(out, "tree ");
    if (!tree->root) {
        fprintf(out, "empty\n");
    } else {
        write_node(tree->root, out);
        fputc('\n', out);
    }
    fprintf(out, "height=%d keys=%d nodes=%d\n", bt_height(tree), tree->key_count,
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
    fprintf(out, "term");
    if (tree->root) {
        fputc(' ', out);
        write_terms(tree->root, out, &first);
    }
    fputc('\n', out);
    /* B 树的叶链是 B+ 树才有的结构，这里固定打出来方便对照。 */
    fprintf(out, "chain=no\n");
}

static int child_slot(const BtNode *parent, const BtNode *child) {
    for (int i = 0; i <= parent->n; i++) {
        if (parent->child[i] == child) {
            return i;
        }
    }
    return -1;
}

static void splice_key(BtNode *parent, int index, int key, BtNode *right) {
    for (int j = parent->n; j > index; j--) {
        parent->key[j] = parent->key[j - 1];
        parent->child[j + 1] = parent->child[j];
    }
    parent->key[index] = key;
    parent->child[index + 1] = right;
    if (right) {
        right->parent = parent;
    }
    parent->n++;
}

/*
 * 临时有 m 个关键字时，第 ⌈m/2⌉ 个上移。左边留 ⌈m/2⌉-1 个，
 * 右边留 m-⌈m/2⌉ 个，两边才都不低于下限。根分裂是长高的唯一方式。
 */
static BtNode *split_node(BtTree *tree, BtNode *node, FILE *tr) {
    int mid = (tree->order + 1) / 2 - 1;
    int up = node->key[mid];
    BtNode *right = bt_new(tree);
    int copied = 0;
    for (int i = mid + 1; i < node->n; i++) {
        right->key[copied++] = node->key[i];
    }
    right->n = copied;
    if (!bt_leaf(node)) {
        int child_n = 0;
        for (int i = mid + 1; i <= node->n; i++) {
            right->child[child_n] = node->child[i];
            right->child[child_n]->parent = right;
            node->child[i] = NULL;
            child_n++;
        }
    }
    node->n = mid;
    if (!node->parent) {
        BtNode *root = bt_new(tree);
        root->n = 1;
        root->key[0] = up;
        root->child[0] = node;
        root->child[1] = right;
        node->parent = root;
        right->parent = root;
        tree->root = root;
    } else {
        int slot = child_slot(node->parent, node);
        splice_key(node->parent, slot, up, right);
    }
    if (tr) {
        fprintf(tr, "split up=%d left=", up);
        bt_write_keys(tr, node->key, node->n);
        fprintf(tr, " right=");
        bt_write_keys(tr, right->key, right->n);
        fprintf(tr, " height=%d\n", bt_height(tree));
    }
    if (node->parent && node->parent->n == tree->order) {
        return node->parent;
    }
    return NULL;
}

static BtNode *descend(BtNode *node, int key, int *found) {
    *found = 0;
    while (node) {
        int i = 0;
        while (i < node->n && key > node->key[i]) {
            i++;
        }
        if (i < node->n && node->key[i] == key) {
            *found = 1;
            return node;
        }
        if (bt_leaf(node)) {
            return node;
        }
        node = node->child[i];
    }
    return NULL;
}

static void put_trace(FILE *tr, const BtNode *node) {
    if (!tr) {
        return;
    }
    fprintf(tr, "put ");
    bt_write_keys(tr, node->key, node->n);
    fputc('\n', tr);
}

int bt_insert(BtTree *tree, int key, FILE *tr) {
    int found = 0;
    BtNode *leaf;
    int index = 0;
    int grew = 0;
    if (tree->key_count >= BT_MAX_N) {
        return BT_RANGE;
    }
    if (!tree->root) {
        tree->root = bt_new(tree);
        tree->root->key[0] = key;
        tree->root->n = 1;
        tree->key_count = 1;
        put_trace(tr, tree->root);
        return BT_OK;
    }
    leaf = descend(tree->root, key, &found);
    if (found) {
        return BT_DUP;
    }
    while (index < leaf->n && leaf->key[index] < key) {
        index++;
    }
    splice_key(leaf, index, key, NULL);
    tree->key_count++;
    while (leaf && leaf->n == tree->order) {
        leaf = split_node(tree, leaf, tr);
        grew = 1;
    }
    if (!grew) {
        put_trace(tr, descend(tree->root, key, &found));
    }
    return BT_OK;
}

static void find_walk(const BtNode *node, int key, FILE *out) {
    while (node) {
        int i = 0;
        fprintf(out, "node ");
        bt_write_keys(out, node->key, node->n);
        fputc('\n', out);
        while (i < node->n && node->key[i] < key) {
            fprintf(out, "%d >\n", node->key[i]);
            i++;
        }
        if (i < node->n && node->key[i] == key) {
            fprintf(out, "%d =\n", key);
            fprintf(out, "hit term=%s\n", bt_leaf(node) ? "yes" : "no");
            return;
        }
        if (i < node->n) {
            fprintf(out, "%d <\n", node->key[i]);
        }
        if (bt_leaf(node)) {
            fprintf(out, "miss term=yes\n");
            return;
        }
        node = node->child[i];
    }
}

int bt_find(const BtTree *tree, int key, FILE *tr) {
    int found = 0;
    BtNode *node;
    if (!tree->root) {
        return BT_MISS;
    }
    if (tr) {
        find_walk(tree->root, key, tr);
    }
    node = descend(tree->root, key, &found);
    (void)node;
    return found ? BT_OK : BT_MISS;
}
