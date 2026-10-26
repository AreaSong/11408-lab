#include "bt_priv.h"

#include "limit.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int parse_key(const char *text, int *out) {
    char *end = NULL;
    long value;
    errno = 0;
    if (!text || text[0] == '\0') {
        return 0;
    }
    value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE || value < BT_KEY_LO || value > BT_KEY_HI) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int abandon(BtNode *node, int rc) {
    bt_free_sub(node);
    return rc;
}

static int read_loaded_keys(BtTree *tree, BtNode *node, char **tok, int *pos, int ntok) {
    while (*pos < ntok && strcmp(tok[*pos], "(") != 0 && strcmp(tok[*pos], ")") != 0) {
        int key = 0;
        if (node->n >= tree->order - 1) {
            return BT_BAD;
        }
        if (!parse_key(tok[*pos], &key)) {
            return BT_FMT;
        }
        node->key[node->n++] = key;
        (*pos)++;
    }
    return BT_OK;
}

static int parse_node(BtTree *tree, char **tok, int *pos, int ntok, BtNode **out) {
    BtNode *node;
    int kids = 0;
    int rc;
    if (*pos >= ntok || strcmp(tok[*pos], "(") != 0) {
        return BT_FMT;
    }
    (*pos)++;
    node = bt_new(tree);
    rc = read_loaded_keys(tree, node, tok, pos, ntok);
    if (rc) {
        return abandon(node, rc);
    }
    while (*pos < ntok && strcmp(tok[*pos], "(") == 0) {
        BtNode *child = NULL;
        if (kids > node->n) {
            return abandon(node, BT_BAD);
        }
        rc = parse_node(tree, tok, pos, ntok, &child);
        if (rc) {
            return abandon(node, rc);
        }
        node->child[kids++] = child;
        child->parent = node;
    }
    if (*pos >= ntok || strcmp(tok[*pos], ")") != 0 || (kids != 0 && kids != node->n + 1)) {
        return abandon(node, (*pos < ntok && strcmp(tok[*pos], ")") == 0) ? BT_BAD : BT_FMT);
    }
    (*pos)++;
    *out = node;
    return BT_OK;
}

static void drop_loaded(BtTree *tree) {
    bt_free_sub(tree->root);
    tree->root = NULL;
    tree->node_count = 0;
    tree->key_count = 0;
}

int bt_load(BtTree *tree, char **tok, int ntok) {
    int pos = 0;
    int count;
    int keys[BT_MAX_N];
    BtNode *root = NULL;
    int rc;
    if (tree->root) {
        return BT_BAD;
    }
    rc = parse_node(tree, tok, &pos, ntok, &root);
    if (rc || pos != ntok) {
        if (!rc) {
            bt_free_sub(root);
        }
        tree->node_count = 0;
        return rc ? rc : BT_FMT;
    }
    tree->root = root;
    count = bt_collect(tree, keys, BT_MAX_N);
    tree->key_count = count < 0 ? 0 : count;
    if (count < 0 || !bt_valid(tree)) {
        drop_loaded(tree);
        return BT_BAD;
    }
    return BT_OK;
}

static int in_bounds(int key, int has_lo, int lo, int has_hi, int hi) {
    if (has_lo && key <= lo) {
        return 0;
    }
    if (has_hi && key >= hi) {
        return 0;
    }
    return 1;
}

static int check_node(const BtTree *tree, const BtNode *node, int is_root, int has_lo, int lo,
                      int has_hi, int hi, int depth, int *leaf_depth, int *keys, int *nodes) {
    int lo_key = limit_min_keys(tree->order, is_root);
    if (!node || node->n < lo_key || node->n > tree->order - 1) {
        return 0;
    }
    (*nodes)++;
    for (int i = 0; i < node->n; i++) {
        if ((i > 0 && node->key[i] <= node->key[i - 1]) ||
            !in_bounds(node->key[i], has_lo, lo, has_hi, hi)) {
            return 0;
        }
        (*keys)++;
    }
    if (bt_leaf(node)) {
        for (int i = 0; i <= node->n; i++) {
            if (node->child[i]) {
                return 0;
            }
        }
        if (*leaf_depth < 0) {
            *leaf_depth = depth;
        }
        return *leaf_depth == depth;
    }
    for (int i = 0; i <= node->n; i++) {
        int child_lo = has_lo;
        int child_hi = has_hi;
        int lo_val = lo;
        int hi_val = hi;
        if (i > 0) {
            child_lo = 1;
            lo_val = node->key[i - 1];
        }
        if (i < node->n) {
            child_hi = 1;
            hi_val = node->key[i];
        }
        if (!node->child[i] || node->child[i]->parent != node) {
            return 0;
        }
        if (!check_node(tree, node->child[i], 0, child_lo, lo_val, child_hi, hi_val, depth + 1,
                        leaf_depth, keys, nodes)) {
            return 0;
        }
    }
    return 1;
}

int bt_valid(const BtTree *tree) {
    int leaf_depth = -1;
    int keys = 0;
    int nodes = 0;
    if (!tree->root) {
        return tree->key_count == 0 && tree->node_count == 0;
    }
    if (tree->root->parent) {
        return 0;
    }
    if (!check_node(tree, tree->root, 1, 0, 0, 0, 0, 1, &leaf_depth, &keys, &nodes)) {
        return 0;
    }
    return keys == tree->key_count && nodes == tree->node_count;
}
