#include "huff.h"

#include <stdio.h>
#include <string.h>

static void clear_tree(Huff *t, const char *names, const int *weight, int n) {
    memset(t, 0, sizeof(*t));
    t->nleaf = n;
    for (int i = 1; i <= n; i++) {
        t->node[i].weight = weight[i - 1];
        t->node[i].name = names[i - 1];
    }
}

static int select_one(const Huff *t, int end, int skip) {
    int best = 0;
    for (int i = 1; i <= end; i++) {
        if (i == skip || t->node[i].parent != 0) {
            continue;
        }
        if (best == 0 || t->node[i].weight < t->node[best].weight) {
            best = i;
        }
    }
    return best;
}

static void label(const Huff *t, int id, char *buf, int cap) {
    if (id <= t->nleaf) {
        snprintf(buf, (size_t)cap, "%c:%d", t->node[id].name, t->node[id].weight);
        return;
    }
    snprintf(buf, (size_t)cap, "#%d:%d", id, t->node[id].weight);
}

static int merge_next(Huff *t, int end, FILE *out) {
    int left = select_one(t, end, 0);
    int right = select_one(t, end, left);
    int id = end + 1;
    char lb[32];
    char rb[32];
    if (left == 0 || right == 0) {
        return 0;
    }
    t->node[id].weight = t->node[left].weight + t->node[right].weight;
    t->node[id].left = left;
    t->node[id].right = right;
    t->node[left].parent = id;
    t->node[right].parent = id;
    if (out != NULL) {
        label(t, left, lb, (int)sizeof lb);
        label(t, right, rb, (int)sizeof rb);
        fprintf(out, "pick %s + %s -> #%d:%d\n", lb, rb, id, t->node[id].weight);
    }
    return id;
}

static void fill_code(Huff *t, int id, char *buf, int len) {
    if (t->node[id].left == 0) {
        buf[len] = '\0';
        memcpy(t->code[id], buf, (size_t)len + 1);
        return;
    }
    buf[len] = '0';
    fill_code(t, t->node[id].left, buf, len + 1);
    buf[len] = '1';
    fill_code(t, t->node[id].right, buf, len + 1);
}

int huff_wpl(const Huff *t) {
    int wpl = 0;
    for (int i = 1; i <= t->nleaf; i++) {
        int depth = 0;
        for (int j = i; t->node[j].parent != 0; j = t->node[j].parent) {
            depth++;
        }
        wpl += t->node[i].weight * depth;
    }
    return wpl;
}

int huff_inside(const Huff *t) {
    int sum = 0;
    for (int i = t->nleaf + 1; i <= t->nnode; i++) {
        sum += t->node[i].weight;
    }
    return sum;
}

int huff_long(const Huff *t, int min_len) {
    int count = 0;
    for (int i = 1; i <= t->nleaf; i++) {
        if ((int)strlen(t->code[i]) >= min_len) {
            count++;
        }
    }
    return count;
}

int huff_degree1(const Huff *t) {
    int count = 0;
    for (int i = 1; i <= t->nnode; i++) {
        int kids = (t->node[i].left != 0) + (t->node[i].right != 0);
        if (kids == 1) {
            count++;
        }
    }
    return count;
}

int huff_height(const Huff *t) {
    int best = 0;
    for (int i = 1; i <= t->nleaf; i++) {
        int len = (int)strlen(t->code[i]);
        if (len > best) {
            best = len;
        }
    }
    return best;
}

static int total_weight(const Huff *t) {
    int sum = 0;
    for (int i = 1; i <= t->nleaf; i++) {
        sum += t->node[i].weight;
    }
    return sum;
}

static void print_avg(FILE *out, int wpl, int total) {
    int hundredths = (int)(((long long)wpl * 1000) / total + 5) / 10;
    fprintf(out, "avg=%d.%02d\n", hundredths / 100, hundredths % 100);
}

static void print_codes(const Huff *t, FILE *out) {
    for (int i = 1; i <= t->nleaf; i++) {
        const char *code = t->code[i][0] == '\0' ? "-" : t->code[i];
        fprintf(out, "code %c=%s len=%d\n", t->node[i].name, code, (int)strlen(t->code[i]));
    }
}

static void print_picks(const Huff *t, FILE *out) {
    for (int id = t->nleaf + 1; id <= t->nnode; id++) {
        char left[32];
        char right[32];
        label(t, t->node[id].left, left, (int)sizeof left);
        label(t, t->node[id].right, right, (int)sizeof right);
        fprintf(out, "pick %s + %s -> #%d:%d\n", left, right, id, t->node[id].weight);
    }
}

static void print_tree(const Huff *t, FILE *out) {
    int wpl = huff_wpl(t);
    int total = total_weight(t);
    fprintf(out, "huff n=%d\nleaf", t->nleaf);
    for (int i = 1; i <= t->nleaf; i++) {
        fprintf(out, " %c=%d", t->node[i].name, t->node[i].weight);
    }
    fputc('\n', out);
    print_picks(t, out);
    print_codes(t, out);
    fprintf(out, "wpl=%d\ninside=%d\n", wpl, huff_inside(t));
    fprintf(out, "total=%d\n", total);
    print_avg(out, wpl, total);
    fprintf(out, "nodes=%d\ndegree1=%d\n", t->nnode, huff_degree1(t));
}

int code_prefix(const char codes[][HUFF_CODE], int n, char *bad_a, char *bad_b) {
    for (int i = 0; i < n; i++) {
        int li = (int)strlen(codes[i]);
        for (int j = 0; j < n; j++) {
            int lj = (int)strlen(codes[j]);
            if (i == j || li > lj) {
                continue;
            }
            if (strncmp(codes[j], codes[i], (size_t)li) == 0) {
                if (bad_a != NULL && bad_b != NULL) {
                    snprintf(bad_a, HUFF_CODE, "%s", codes[i]);
                    snprintf(bad_b, HUFF_CODE, "%s", codes[j]);
                }
                return HUFF_PREFIX;
            }
        }
    }
    return HUFF_OK;
}

static void report_prefix(const Huff *t, FILE *out) {
    char codes[HUFF_MAX][HUFF_CODE];
    char bad_a[HUFF_CODE];
    char bad_b[HUFF_CODE];
    for (int i = 0; i < t->nleaf; i++) {
        snprintf(codes[i], HUFF_CODE, "%s", t->code[i + 1]);
    }
    if (code_prefix(codes, t->nleaf, bad_a, bad_b) == HUFF_OK) {
        fprintf(out, "prefix=ok\n");
        return;
    }
    fprintf(out, "prefix=bad %s %s\n", bad_a, bad_b);
}

int huff_build(Huff *t, const char *names, const int *weight, int n, FILE *out) {
    char buf[HUFF_CODE];
    if (n < 1 || n > HUFF_MAX) {
        return HUFF_RANGE;
    }
    clear_tree(t, names, weight, n);
    t->nnode = n == 1 ? 1 : 2 * n - 1;
    if (n > 1) {
        for (int end = n; end < t->nnode; end++) {
            if (merge_next(t, end, NULL) == 0) {
                return HUFF_BAD;
            }
        }
    }
    fill_code(t, t->nnode, buf, 0);
    if (out != NULL) {
        print_tree(t, out);
        report_prefix(t, out);
    }
    return HUFF_OK;
}

int code_decode(const char *bits, const char *names, const char codes[][HUFF_CODE], int n,
                char *text, int cap) {
    int pos = 0;
    int used = 0;
    int total = (int)strlen(bits);
    if (code_prefix(codes, n, NULL, NULL) != HUFF_OK) {
        return HUFF_PREFIX;
    }
    while (pos < total) {
        int match = -1;
        int len = 0;
        for (int i = 0; i < n; i++) {
            int clen = (int)strlen(codes[i]);
            if (clen == 0 || pos + clen > total) {
                continue;
            }
            if (strncmp(bits + pos, codes[i], (size_t)clen) == 0) {
                match = i;
                len = clen;
                break;
            }
        }
        if (match < 0 || used + 1 >= cap) {
            return HUFF_BAD;
        }
        text[used++] = names[match];
        pos += len;
    }
    text[used] = '\0';
    return HUFF_OK;
}

static int add_leaf(int *leaf, int *nleaf, int weight) {
    if (weight <= 0 || *nleaf >= HUFF_MAX) {
        return 0;
    }
    leaf[(*nleaf)++] = weight;
    return 1;
}

static int add_branch(const int *path, int from, int npath, int *leaf, int *nleaf) {
    if (from == npath - 1) {
        return add_leaf(leaf, nleaf, path[from]);
    }
    for (int i = from; i < npath - 1; i++) {
        if (!add_leaf(leaf, nleaf, path[i] - path[i + 1])) {
            return 0;
        }
    }
    return add_leaf(leaf, nleaf, path[npath - 1]);
}

static int path_positive(const int *path, int npath) {
    if (npath < 2 || npath > 8) {
        return 0;
    }
    for (int i = 0; i < npath; i++) {
        if (path[i] <= 0) {
            return 0;
        }
    }
    return 1;
}

static int diverge_at(const int *a, int na, const int *b, int nb) {
    int i = 0;
    while (i < na && i < nb && a[i] == b[i]) {
        i++;
    }
    return i;
}

static int cover_path(const Huff *t, const int *path, int npath) {
    int have[HUFF_NODE];
    for (int i = 0; i < t->nnode; i++) {
        have[i] = t->node[i + 1].weight;
    }
    for (int i = 0; i < npath; i++) {
        int found = 0;
        for (int j = 0; j < t->nnode; j++) {
            if (have[j] == path[i]) {
                have[j] = -1;
                found = 1;
                break;
            }
        }
        if (!found) {
            return 0;
        }
    }
    return 1;
}

static int collect_leaves(const int *a, int na, const int *b, int nb, int d, int *leaf) {
    int nleaf = 0;
    for (int i = 0; i < d - 1; i++) {
        if (!add_leaf(leaf, &nleaf, a[i] - a[i + 1])) {
            return -1;
        }
    }
    if (!add_branch(a, d, na, leaf, &nleaf) || !add_branch(b, d, nb, leaf, &nleaf)) {
        return -1;
    }
    return nleaf;
}

int huff_paths_same(const int *a, int na, const int *b, int nb) {
    int d;
    int leaf[HUFF_MAX];
    int nleaf;
    char names[HUFF_MAX + 1];
    Huff tree;
    if (!path_positive(a, na) || !path_positive(b, nb) || a[0] != b[0]) {
        return 0;
    }
    d = diverge_at(a, na, b, nb);
    if (d == 0 || d >= na || d >= nb || a[d - 1] != a[d] + b[d]) {
        return 0;
    }
    nleaf = collect_leaves(a, na, b, nb, d, leaf);
    if (nleaf < 2) {
        return 0;
    }
    for (int i = 0; i < nleaf; i++) {
        names[i] = (char)('A' + i);
    }
    names[nleaf] = '\0';
    if (huff_build(&tree, names, leaf, nleaf, NULL) != HUFF_OK) {
        return 0;
    }
    if (tree.node[tree.nnode].weight != a[0]) {
        return 0;
    }
    return cover_path(&tree, a, na) && cover_path(&tree, b, nb);
}
