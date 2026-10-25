#include "forest.h"

#include <ctype.h>
#include <string.h>

void forest_init(Forest *f) {
    memset(f, 0, sizeof *f);
}

static int push_ch(char *buf, int cap, int n, char ch) {
    if (buf == NULL) {
        return n + 1;
    }
    if (n < 0 || n + 1 >= cap) {
        return -1;
    }
    buf[n] = ch;
    buf[n + 1] = '\0';
    return n + 1;
}

static int alloc_node(Forest *f, char data, int parent) {
    if (f->n + 1 >= FOR_MAX) {
        return 0;
    }
    f->n++;
    f->node[f->n].data = data;
    f->node[f->n].parent = parent;
    return f->n;
}

static int name_used(const Forest *f, char data) {
    for (int i = 1; i <= f->n; i++) {
        if (f->node[i].data == data) {
            return 1;
        }
    }
    return 0;
}

static void skip_sp(const char *s, int *i) {
    while (s[*i] == ' ' || s[*i] == '\t') {
        (*i)++;
    }
}

static int parse_tree(Forest *f, const char *s, int *i, int parent, int *out);

static int parse_kids(Forest *f, const char *s, int *i, int id) {
    int prev = 0;
    for (;;) {
        int child = 0;
        int err = parse_tree(f, s, i, id, &child);
        if (err != THR_OK) {
            return err;
        }
        if (prev == 0) {
            f->node[id].first = child;
        } else {
            f->node[prev].next = child;
        }
        prev = child;
        skip_sp(s, i);
        if (s[*i] == ',') {
            (*i)++;
            continue;
        }
        if (s[*i] == ')') {
            (*i)++;
            return THR_OK;
        }
        return THR_FORMAT;
    }
}

static int parse_tree(Forest *f, const char *s, int *i, int parent, int *out) {
    unsigned char ch;
    int id;
    int err;
    skip_sp(s, i);
    ch = (unsigned char)s[*i];
    if (!isalpha(ch)) {
        return THR_FORMAT;
    }
    if (name_used(f, (char)ch)) {
        return THR_DUP;
    }
    id = alloc_node(f, (char)ch, parent);
    if (id == 0) {
        return THR_SIZE;
    }
    (*i)++;
    skip_sp(s, i);
    if (s[*i] != '(') {
        *out = id;
        return THR_OK;
    }
    (*i)++;
    skip_sp(s, i);
    if (s[*i] == ')' || s[*i] == '\0') {
        return THR_FORMAT;
    }
    err = parse_kids(f, s, i, id);
    if (err != THR_OK) {
        return err;
    }
    *out = id;
    return THR_OK;
}

static void link_roots(Forest *f) {
    for (int i = 1; i < f->ntree; i++) {
        f->node[f->root[i]].next = f->root[i + 1];
    }
}

int forest_parse(Forest *f, const char *s) {
    int i = 0;
    forest_init(f);
    if (s == NULL) {
        return THR_EMPTY;
    }
    skip_sp(s, &i);
    if (s[i] == '\0') {
        return THR_EMPTY;
    }
    while (s[i] != '\0') {
        int id = 0;
        int err = parse_tree(f, s, &i, -1, &id);
        if (err != THR_OK) {
            forest_init(f);
            return err;
        }
        if (f->ntree + 1 >= FOR_MAX) {
            forest_init(f);
            return THR_SIZE;
        }
        f->root[++f->ntree] = id;
        skip_sp(s, &i);
    }
    link_roots(f);
    return THR_OK;
}

static int same_chars(const char *pre, const char *inord) {
    int n = (int)strlen(pre);
    if (n == 0 || n != (int)strlen(inord)) {
        return 0;
    }
    for (int i = 0; i < n; i++) {
        int count = 0;
        if (!isgraph((unsigned char)pre[i]) || pre[i] == '#') {
            return 0;
        }
        for (int j = 0; j < n; j++) {
            if (pre[j] == pre[i] && j != i) {
                return 0;
            }
            if (inord[j] == pre[i]) {
                count++;
            }
        }
        if (count != 1) {
            return 0;
        }
    }
    return 1;
}

static int build_bi(Forest *f, const char *pre, int *pi, const char *inord, int lo, int hi, int *out) {
    char ch;
    int pos = -1;
    int id;
    int left = 0;
    int right = 0;
    int err;
    if (lo > hi) {
        *out = 0;
        return THR_OK;
    }
    ch = pre[*pi];
    if (ch == '\0') {
        return THR_SHORT;
    }
    (*pi)++;
    for (int i = lo; i <= hi; i++) {
        if (inord[i] == ch) {
            pos = i;
            break;
        }
    }
    if (pos < 0) {
        return THR_MISMATCH;
    }
    id = alloc_node(f, ch, 0);
    if (id == 0) {
        return THR_SIZE;
    }
    err = build_bi(f, pre, pi, inord, lo, pos - 1, &left);
    if (err != THR_OK) {
        return err;
    }
    err = build_bi(f, pre, pi, inord, pos + 1, hi, &right);
    if (err != THR_OK) {
        return err;
    }
    f->node[id].first = left;
    f->node[id].next = right;
    *out = id;
    return THR_OK;
}

static void bind_parent(Forest *f, int id, int parent) {
    while (id != 0) {
        f->node[id].parent = parent;
        bind_parent(f, f->node[id].first, id);
        id = f->node[id].next;
    }
}

int forest_from_bi(Forest *f, const char *pre, const char *inord) {
    int pi = 0;
    int root = 0;
    int err;
    int id;
    forest_init(f);
    if (pre == NULL || inord == NULL || pre[0] == '\0' || inord[0] == '\0') {
        return THR_EMPTY;
    }
    if (!same_chars(pre, inord)) {
        return THR_MISMATCH;
    }
    err = build_bi(f, pre, &pi, inord, 0, (int)strlen(inord) - 1, &root);
    if (err != THR_OK || pre[pi] != '\0') {
        forest_init(f);
        return err != THR_OK ? err : THR_EXTRA;
    }
    bind_parent(f, root, -1);
    id = root;
    while (id != 0) {
        f->root[++f->ntree] = id;
        id = f->node[id].next;
    }
    return THR_OK;
}

int forest_find(const Forest *f, char data) {
    for (int i = 1; i <= f->n; i++) {
        if (f->node[i].data == data) {
            return i;
        }
    }
    return 0;
}

int forest_bin_parent(const Forest *f, int id) {
    if (id <= 0) {
        return 0;
    }
    for (int i = 1; i <= f->n; i++) {
        if (f->node[i].first == id || f->node[i].next == id) {
            return i;
        }
    }
    return 0;
}

static int are_siblings(const Forest *f, int a, int b) {
    if (a <= 0 || b <= 0 || a == b) {
        return 0;
    }
    if (f->node[a].parent == -1 && f->node[b].parent == -1) {
        return 1;
    }
    return f->node[a].parent > 0 && f->node[a].parent == f->node[b].parent;
}

void forest_grand_stats(const Forest *f, GrandStats *stats) {
    memset(stats, 0, sizeof *stats);
    for (int u = 1; u <= f->n; u++) {
        for (int v = 1; v <= f->n; v++) {
            int mid;
            int pv;
            if (u == v) {
                continue;
            }
            mid = forest_bin_parent(f, v);
            if (mid == 0 || forest_bin_parent(f, mid) != u) {
                continue;
            }
            stats->total++;
            pv = f->node[v].parent;
            if (pv == u) {
                stats->as_parent++;
            } else if (are_siblings(f, u, v)) {
                stats->as_sibling++;
            } else if (pv > 0 && are_siblings(f, u, pv)) {
                stats->as_uncle++;
            } else {
                stats->as_other++;
            }
            if (are_siblings(f, f->node[u].parent, pv)) {
                stats->as_iii++;
            }
        }
    }
}

int forest_leaves(const Forest *f) {
    int n = 0;
    for (int i = 1; i <= f->n; i++) {
        if (f->node[i].first == 0) {
            n++;
        }
    }
    return n;
}

int forest_right_null(const Forest *f) {
    int n = 0;
    for (int i = 1; i <= f->n; i++) {
        if (f->node[i].next == 0) {
            n++;
        }
    }
    return n;
}

int forest_bin_leaf(const Forest *f) {
    int n = 0;
    for (int i = 1; i <= f->n; i++) {
        if (f->node[i].first == 0 && f->node[i].next == 0) {
            n++;
        }
    }
    return n;
}

int forest_deg1(const Forest *f) {
    int n = 0;
    for (int i = 1; i <= f->n; i++) {
        int empty = (f->node[i].first == 0) + (f->node[i].next == 0);
        if (empty == 1) {
            n++;
        }
    }
    return n;
}

int forest_walk_bin(const Forest *f, int id, int order, char *buf, int cap, int n) {
    if (id == 0) {
        return n;
    }
    if (order == 0) {
        n = push_ch(buf, cap, n, f->node[id].data);
    }
    n = forest_walk_bin(f, f->node[id].first, order, buf, cap, n);
    if (order == 1) {
        n = push_ch(buf, cap, n, f->node[id].data);
    }
    n = forest_walk_bin(f, f->node[id].next, order, buf, cap, n);
    if (order == 2) {
        n = push_ch(buf, cap, n, f->node[id].data);
    }
    return n;
}

int forest_walk_tree(const Forest *f, int id, int post, char *buf, int cap, int n) {
    int child;
    if (id == 0) {
        return n;
    }
    if (!post) {
        n = push_ch(buf, cap, n, f->node[id].data);
    }
    for (child = f->node[id].first; child != 0; child = f->node[child].next) {
        n = forest_walk_tree(f, child, post, buf, cap, n);
    }
    if (post) {
        n = push_ch(buf, cap, n, f->node[id].data);
    }
    return n;
}

static int forest_order(const Forest *f, int post, char *buf, int cap) {
    int n = 0;
    buf[0] = '\0';
    for (int i = 1; i <= f->ntree; i++) {
        n = forest_walk_tree(f, f->root[i], post, buf, cap, n);
        if (n < 0) {
            return -1;
        }
    }
    return THR_OK;
}

int forest_seq(const Forest *f, char *pre, char *inord, char *post, int cap) {
    if (forest_order(f, 0, pre, cap) != THR_OK) {
        return -1;
    }
    if (forest_order(f, 1, inord, cap) != THR_OK) {
        return -1;
    }
    post[0] = '\0';
    if (f->ntree == 0) {
        return THR_OK;
    }
    return forest_walk_bin(f, f->root[1], 2, post, cap, 0) < 0 ? -1 : THR_OK;
}
