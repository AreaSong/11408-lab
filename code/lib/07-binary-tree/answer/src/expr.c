#include "expr.h"

#include <string.h>

#define EX_MAX 64

typedef struct {
    char text[10];
    int left;
    int right;
} ExNode;

typedef struct {
    ExNode node[EX_MAX];
    int n;
} ExTree;

typedef struct {
    char **argv;
    int argc;
    int i;
    ExTree *tree;
} ExIn;

typedef struct {
    char *buf;
    int cap;
    int n;
    FILE *trace;
} ExOut;

static int take(ExIn *in) {
    if (in->i >= in->argc) {
        return -2;
    }
    char *tok = in->argv[in->i++];
    if (strcmp(tok, "#") == 0) {
        return 0;
    }
    if (strlen(tok) >= 10) {
        return -1;
    }
    if (in->tree->n + 1 >= EX_MAX) {
        return -3;
    }
    int id = ++in->tree->n;
    memset(&in->tree->node[id], 0, sizeof in->tree->node[id]);
    memcpy(in->tree->node[id].text, tok, strlen(tok) + 1);
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

static int add_str(ExOut *out, const char *s) {
    while (*s != '\0') {
        if (out->n + 1 >= out->cap) {
            return -1;
        }
        out->buf[out->n++] = *s++;
    }
    out->buf[out->n] = '\0';
    return 0;
}

static int walk(const ExTree *t, int id, int deep, ExOut *out) {
    int leaf = t->node[id].left == 0 && t->node[id].right == 0;
    if (leaf) {
        if (out->trace != NULL) {
            fprintf(out->trace, "leaf %s deep=%d\n", t->node[id].text, deep);
        }
        return add_str(out, t->node[id].text);
    }
    int paren = deep > 1;
    if (paren) {
        if (out->trace != NULL) {
            fprintf(out->trace, "paren ( deep=%d op=%s\n", deep, t->node[id].text);
        }
        if (add_str(out, "(") < 0) {
            return -1;
        }
    }
    if (t->node[id].left != 0 && walk(t, t->node[id].left, deep + 1, out) < 0) {
        return -1;
    }
    if (out->trace != NULL) {
        fprintf(out->trace, "op %s deep=%d\n", t->node[id].text, deep);
    }
    if (add_str(out, t->node[id].text) < 0) {
        return -1;
    }
    if (t->node[id].right != 0 && walk(t, t->node[id].right, deep + 1, out) < 0) {
        return -1;
    }
    if (!paren) {
        return 0;
    }
    if (out->trace != NULL) {
        fprintf(out->trace, "paren ) deep=%d op=%s\n", deep, t->node[id].text);
    }
    return add_str(out, ")");
}

int expr_eval(int argc, char **argv, char *buf, int cap, FILE *trace) {
    if (argc < 1 || cap < 2) {
        return -2;
    }
    ExTree tree;
    tree.n = 0;
    ExIn in = {argv, argc, 0, &tree};
    int root = take(&in);
    if (root < 0) {
        return root;
    }
    if (in.i != argc) {
        return -4;
    }
    if (root == 0) {
        return -2;
    }
    buf[0] = '\0';
    ExOut out = {buf, cap, 0, trace};
    if (walk(&tree, root, 1, &out) < 0) {
        return -3;
    }
    if (trace != NULL) {
        fprintf(trace, "infix %s\n", buf);
    }
    return 0;
}
