#include "loser.h"

#include "balance.h"

#include <string.h>

#define LOSER_MIN_KEY (EXT_KEY_MIN - 1)
#define LOSER_MAX_KEY (EXT_KEY_MAX + 1)

typedef struct {
    int key[LOSER_K + 1];
    int ls[LOSER_K];
    int at[LOSER_K];
    int k;
    int cmp;
} Tree;

/* 更大的留下当败者。相等时当前上来的这个获胜，建树从大段号收到小段号，段号小的会赢。 */
static void tree_adjust(Tree *tree, int s) {
    int parent = (s + tree->k) / 2;
    while (parent > 0) {
        tree->cmp++;
        if (tree->key[s] > tree->key[tree->ls[parent]]) {
            int loser = s;
            s = tree->ls[parent];
            tree->ls[parent] = loser;
        }
        parent /= 2;
    }
    tree->ls[0] = s;
}

static int segment_ok(const LoseIn *in, int s) {
    int i;
    if (in->len[s] < 1 || in->len[s] > LOSER_SEG) {
        return 0;
    }
    for (i = 0; i < in->len[s]; i++) {
        int key = in->data[s][i];
        if (key < EXT_KEY_MIN || key > EXT_KEY_MAX) {
            return 0;
        }
        if (i > 0 && key < in->data[s][i - 1]) {
            return 0;
        }
    }
    return 1;
}

static int tree_init(Tree *tree, const LoseIn *in) {
    int s;
    tree->k = in->k;
    tree->cmp = 0;
    for (s = 0; s < tree->k; s++) {
        if (!segment_ok(in, s)) {
            return -1;
        }
        tree->at[s] = 0;
        tree->key[s] = in->data[s][0];
    }
    /* 极小哨兵先占着所有内部结点。从大段号调到小段号，真实关键字会把哨兵挤出冠军位置。 */
    tree->key[tree->k] = LOSER_MIN_KEY;
    for (s = 0; s < tree->k; s++) {
        tree->ls[s] = tree->k;
    }
    for (s = tree->k - 1; s >= 0; s--) {
        tree_adjust(tree, s);
    }
    return 0;
}

static void tree_advance(Tree *tree, const LoseIn *in, int s) {
    tree->at[s]++;
    if (tree->at[s] >= in->len[s]) {
        tree->key[s] = LOSER_MAX_KEY;
    } else {
        tree->key[s] = in->data[s][tree->at[s]];
    }
    tree_adjust(tree, s);
}

int loser_merge(const LoseIn *in, LoseResult *out) {
    Tree tree;
    SelectBound bound;
    int s;
    if (in->k < 2 || in->k > LOSER_K || tree_init(&tree, in) != 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->k = in->k;
    out->winner_seg = tree.ls[0];
    out->winner_key = tree.key[tree.ls[0]];
    for (s = 0; s < in->k; s++) {
        out->ls[s] = tree.ls[s];
    }
    while (tree.key[tree.ls[0]] != LOSER_MAX_KEY) {
        int seg = tree.ls[0];
        if (out->n >= LOSER_OUT) {
            return -1;
        }
        out->out[out->n] = tree.key[seg];
        out->from[out->n] = seg;
        out->n++;
        tree_advance(&tree, in, seg);
    }
    if (select_bounds(out->n, in->k, &bound) != 0) {
        return -1;
    }
    out->cmp = tree.cmp;
    out->path = bound.path;
    out->formula = bound.formula;
    out->naive_rest = bound.naive_rest;
    out->naive_all = bound.naive_all;
    return 0;
}

void loser_print(const LoseResult *result, FILE *out) {
    int i;
    fprintf(out, "winner_seg=%d\n", result->winner_seg);
    fprintf(out, "winner_key=%d\n", result->winner_key);
    fprintf(out, "ls");
    for (i = 0; i < result->k; i++) {
        fprintf(out, " %d", result->ls[i]);
    }
    fprintf(out, "\nout");
    for (i = 0; i < result->n; i++) {
        fprintf(out, " %d", result->out[i]);
    }
    fprintf(out, "\ncmp=%d\n", result->cmp);
    fprintf(out, "path=%d\n", result->path);
    fprintf(out, "formula=%lld\n", result->formula);
    fprintf(out, "naive_rest=%lld\n", result->naive_rest);
    fprintf(out, "naive_all=%lld\n", result->naive_all);
}
