#include "wpl.h"

#include "tree.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    char **argv;
    int argc;
    int i;
    BiTree *tree;
} WplIn;

static int take(WplIn *in) {
    if (in->i >= in->argc) {
        return -2;
    }
    char *tok = in->argv[in->i++];
    if (strcmp(tok, "#") == 0) {
        return 0;
    }
    char *end = NULL;
    long value = strtol(tok, &end, 10);
    if (end == tok || *end != '\0' || value < 0 || value > 1000000) {
        return -1;
    }
    int id = tree_alloc(in->tree, (int)value);
    if (id == 0) {
        return -3;
    }
    int left = take(in);
    if (left < 0) {
        return left;
    }
    int right = take(in);
    if (right < 0) {
        return right;
    }
    in->tree->node[id].left = left;
    in->tree->node[id].right = right;
    return id;
}

static long long add_wpl(const BiTree *t, int id, int edge, FILE *trace) {
    if (id == 0) {
        return 0;
    }
    int leaf = t->node[id].left == 0 && t->node[id].right == 0;
    if (!leaf) {
        if (trace != NULL) {
            fprintf(trace, "inner weight=%d edge=%d\n", t->node[id].data, edge);
        }
        return add_wpl(t, t->node[id].left, edge + 1, trace) +
               add_wpl(t, t->node[id].right, edge + 1, trace);
    }
    long long add = (long long)t->node[id].data * edge;
    if (trace != NULL) {
        fprintf(trace, "leaf weight=%d edge=%d level=%d add=%lld\n", t->node[id].data, edge, edge + 1, add);
    }
    return add;
}

int wpl_eval(int argc, char **argv, int *sum, FILE *trace) {
    if (argc < 1) {
        return -2;
    }
    BiTree tree;
    tree_init(&tree);
    WplIn in = {argv, argc, 0, &tree};
    int root = take(&in);
    if (root < 0) {
        return root;
    }
    if (in.i != argc) {
        return -4;
    }
    tree.root = root;
    if (trace != NULL) {
        fprintf(trace, "rule path_edge root=0 level root=1 use=edge\n");
    }
    long long total = add_wpl(&tree, root, 0, trace);
    if (total > 2147483647LL) {
        return -3;
    }
    *sum = (int)total;
    if (trace != NULL) {
        fprintf(trace, "wpl=%d\n", *sum);
    }
    return 0;
}
